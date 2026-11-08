// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "luvk/Core/Common.hpp"
#include "luvk/Resources/Buffer.hpp"

namespace luvk
{
    class Context;

    /** A range of per-frame memory: CPU-writable now, GPU-readable by the frame being recorded. */
    struct FrameAllocation
    {
        /** Mapped bytes of the range. */
        std::byte* Data = nullptr;

        /** Buffer containing the range (for vertex/index binding or graph-less copies). */
        VkBuffer Buffer = nullptr;

        /** Byte offset of the range inside Buffer. */
        DeviceSize Offset = 0U;

        /** Size of the range in bytes. */
        DeviceSize Size = 0U;

        /** Device address of the range start (for shaders reading through buffer device addresses). */
        DeviceAddress Address = 0U;

        /** True when the range is valid. */
        [[nodiscard]] explicit operator bool() const noexcept { return Data != nullptr; }

        template <typename T>
            requires std::is_trivially_copyable_v<T>
        /** Copies Values to the given byte offset of the range (bounds-checked). */
        void Write(const std::span<const T> Values, const DeviceSize ByteOffset = 0U) const
        {
            if (ByteOffset > Size || Values.size_bytes() > Size - ByteOffset)
            {
                throw std::out_of_range("[luvk] FrameAllocation::Write out of range.");
            }
            std::memcpy(Data + ByteOffset, std::data(Values), Values.size_bytes());
        }
    };

    /**
     * Linear allocator for data written by the CPU every frame (UI geometry, debug lines, instance transforms,
     * constants larger than the push constant block...). Each frame slot owns its blocks of MemoryUsage::Stream
     * memory, reset when the Renderer begins that slot again, i.e. once the GPU finished reading it: no per-frame
     * allocation, no staging copy and no synchronisation to think about.
     *
     *     const FrameAllocation Vertices = Frame.Allocator.Upload(std::span(MyVertices));
     *     Cmd.PushConstants(Push{.Vertices = Vertices.Address});
     */
    class LUVK_API FrameAllocator
    {
        /** One block of stream memory. */
        struct Block
        {
            /** Backing buffer (mapped). */
            Buffer Memory;

            /** Bytes handed out from the block this frame. */
            DeviceSize Used = 0U;
        };

        /** Owning context. */
        Context& m_Context;

        /** Size of a new block, grown to fit larger requests. */
        DeviceSize m_BlockSize;

        /** Blocks of each frame slot. */
        std::vector<std::vector<Block>> m_Slots;

        /** Slot allocations currently come from. */
        std::uint32_t m_Current = 0U;

    public:
        /** Constructor. One block set per frame slot; blocks are created on first use. */
        FrameAllocator(Context& Ctx, std::uint32_t FramesInFlight, DeviceSize BlockSize = 4ULL * 1024ULL * 1024ULL);

        /** Non-copyable. */
        FrameAllocator(const FrameAllocator&) = delete;

        /** Non-copyable. */
        FrameAllocator& operator=(const FrameAllocator&) = delete;

        /** Starts allocating from FrameSlot (the GPU must be done with it); a slot that needed several blocks gets one large block. */
        void BeginFrame(std::uint32_t FrameSlot);

        /** Makes this frame's writes visible to the GPU (non-coherent memory only); called before the frame is submitted. */
        void EndFrame() const;

        /** Returns Size bytes aligned to Alignment (a power of two), valid for the current frame only. */
        [[nodiscard]] FrameAllocation Allocate(DeviceSize Size, DeviceSize Alignment = 16U);

        template <typename T>
            requires std::is_trivially_copyable_v<T>
        /** Allocates room for Values, copies them and returns the range. */
        [[nodiscard]] FrameAllocation Upload(const std::span<const T> Values, const DeviceSize Alignment = 16U)
        {
            const FrameAllocation Range = Allocate(std::max<DeviceSize>(1U, Values.size_bytes()), Alignment);
            Range.Write(Values);
            return Range;
        }

        /** Bytes allocated in the current frame. */
        [[nodiscard]] DeviceSize UsedBytes() const noexcept;
    };
}
