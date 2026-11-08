// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>
#include "luvk/Core/Context.hpp"
#include "luvk/Render/CommandList.hpp"
#include "luvk/Render/RenderGraph.hpp"
#include "luvk/Resources/Buffer.hpp"

namespace luvk
{
    /**
     * Bounded GPU -> CPU readback: Depth host buffers of Capacity elements each. Request() declares a copy pass from a graph
     * buffer into the next free slot; Collect() returns the oldest slot the GPU has finished writing, or nothing. A slot
     * is complete once the graphics timeline reaches the value of the frame that copied into it (the graph stamps it on
     * the slot's buffer at submission), so nothing ever waits and nothing grows.
     */
    template <typename T>
        requires std::is_trivially_copyable_v<T>
    class ReadbackRing
    {
        /** One host buffer and what was requested into it. */
        struct Slot
        {
            /** Host-visible destination. */
            Buffer Memory;

            /** Elements copied by the pending request. */
            std::uint32_t Count = 0U;

            /** True between Request and Collect. */
            bool Pending = false;
        };

        /** Owning context (completion queries). */
        Context* m_Context = nullptr;

        /** Ring of slots. */
        std::vector<Slot> m_Slots;

        /** Elements per slot. */
        std::uint32_t m_Capacity = 0U;

        /** Next slot Request writes. */
        std::uint32_t m_Next = 0U;

        /** Oldest pending slot (Collect reads it first). */
        std::uint32_t m_Oldest = 0U;

    public:
        /** Default constructor (empty ring). */
        ReadbackRing() = default;

        /** Constructor: Depth slots of Capacity elements in readback memory. */
        ReadbackRing(Context& Ctx, const std::uint32_t Capacity, const std::uint32_t Depth = 3U, const std::string_view Name = "luvk readback")
            : m_Context(&Ctx),
              m_Capacity(Capacity)
        {
            m_Slots.resize(Depth < 1U ? 1U : Depth);
            const std::string Label(Name);
            for (Slot& Entry : m_Slots)
            {
                Entry.Memory = Buffer(Ctx, {.Size = static_cast<DeviceSize>(Capacity) * sizeof(T), .Memory = MemoryUsage::Readback, .Name = Label});
            }
        }

        /** Elements per slot. */
        [[nodiscard]] std::uint32_t Capacity() const noexcept { return m_Capacity; }

        /** True when every slot waits for the GPU (Request would drop the copy). */
        [[nodiscard]] bool IsFull() const noexcept { return m_Slots.empty() || m_Slots[m_Next].Pending; }

        /**
         * Declares a copy of Count elements (clamped to the capacity) from Source at SourceOffset bytes into the next slot.
         * Returns false, and records nothing, when every slot is still pending.
         */
        bool Request(RenderGraph& Graph, const BufferHandle Source, const std::uint32_t Count, const DeviceSize SourceOffset = 0U)
        {
            if (IsFull() || Count == 0U)
            {
                return false;
            }

            Slot&               Entry       = m_Slots[m_Next];
            const BufferHandle  Destination = Graph.Import(Entry.Memory);
            const std::uint32_t Elements    = Count < m_Capacity ? Count : m_Capacity;
            const DeviceSize    Bytes       = static_cast<DeviceSize>(Elements) * sizeof(T);

            Graph.AddPass("Readback",
                          PassType::Transfer,
                          [&](PassBuilder& Pass) { Pass.Use(Source, Access::TransferSrc).Use(Destination, Access::TransferDst); },
                          [Source, Destination, SourceOffset, Bytes](CommandList& Cmd)
                          {
                              const BufferCopy Region{.SourceOffset = SourceOffset, .Size = Bytes};
                              Cmd.CopyBuffer(Source, Destination, std::span<const BufferCopy>(&Region, 1U));
                          });
            Graph.SetFinalAccess(Destination, Access::HostRead);

            Entry.Count   = Elements;
            Entry.Pending = true;
            m_Next        = (m_Next + 1U) % static_cast<std::uint32_t>(m_Slots.size());
            return true;
        }

        /** The oldest completed request, valid until the next Collect; nothing when the GPU has not finished one yet. */
        [[nodiscard]] std::optional<std::span<const T>> Collect()
        {
            if (m_Slots.empty())
            {
                return std::nullopt;
            }

            Slot& Entry = m_Slots[m_Oldest];
            if (!Entry.Pending)
            {
                return std::nullopt;
            }

            const std::uint64_t Submitted = Entry.Memory.State().LastGraphicsUse;
            if (Submitted == 0U || m_Context->GraphicsQueue().CompletedValue() < Submitted)
            {
                return std::nullopt;
            }

            Entry.Pending = false;
            m_Oldest      = (m_Oldest + 1U) % static_cast<std::uint32_t>(m_Slots.size());

            const DeviceSize Bytes = static_cast<DeviceSize>(Entry.Count) * sizeof(T);
            Entry.Memory.Invalidate(0U, Bytes);
            return std::span<const T>(reinterpret_cast<const T*>(Entry.Memory.Mapped().data()), Entry.Count);
        }
    };
}
