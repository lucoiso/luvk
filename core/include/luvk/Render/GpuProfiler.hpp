// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    class Context;

    /** GPU timing of one named pass. */
    struct PassTiming
    {
        /** Pass name. */
        std::string Name;

        /** GPU time of the most recent frame in milliseconds. */
        double Milliseconds = 0.0;

        /** Exponentially smoothed average GPU time in milliseconds. */
        double Average = 0.0;
    };

    /**
     * GPU timestamps per render graph pass. One query pool per frame in flight; results of a slot are read
     * back when the slot is reused (its previous submission is known to be complete), so it never stalls.
     * The Renderer drives it automatically when RendererDesc::GpuTimings is set.
     */
    class LUVK_API GpuProfiler
    {
        /** Per frame-in-flight query pool state. */
        struct Slot
        {
            /** Timestamp query pool (m_MaxQueries queries). */
            VkQueryPool Pool = VK_NULL_HANDLE;

            /** Names of the passes recorded in the current frame, in query order. */
            std::vector<std::string> Names;

            /** Number of queries used in the current frame (two per pass plus two for the frame). */
            std::uint32_t Queries = 0U;

            /** True while the pool has an in-flight submission with unread results. */
            bool Pending = false;
        };

        /** Owning context. */
        Context&                m_Context;

        /** One slot per frame in flight. */
        std::vector<Slot> m_Slots;

        /** Queries per pool: 2 * MaxPasses + 2 (frame begin/end). */
        std::uint32_t m_MaxQueries = 0U;

        /** Slot index used by the frame being recorded. */
        std::uint32_t m_Current = 0U;

        /** Device timestamp period in nanoseconds per tick. */
        double m_PeriodNs = 0.0;

        /** Mask of the timestamp bits the graphics queue writes (the rest is garbage). */
        std::uint64_t m_TimestampMask = 0U;

        /** False when the graphics queue does not support timestamps. */
        bool m_Supported = false;

        /** Latest per-pass timings, updated on Collect. */
        std::vector<PassTiming> m_Results;

        /** Whole-frame timing (first to last command of the frame). */
        PassTiming              m_Frame{.Name = "Frame (GPU)"};

    public:
        /** Constructor; creates one query pool per slot (SlotCount should match the frames in flight). */
        GpuProfiler(Context& Ctx, std::uint32_t SlotCount, std::uint32_t MaxPasses = 128U);

        /** Destructor. */
        ~GpuProfiler();

        /** Non-copyable. */
        GpuProfiler(const GpuProfiler&)            = delete;

        /** Non-copyable. */
        GpuProfiler& operator=(const GpuProfiler&) = delete;

        /** True when the graphics queue supports GPU timestamps. */
        [[nodiscard]] bool IsSupported() const noexcept { return m_Supported; }

        /** Collects the previous results of SlotIndex and starts a new frame in Commands. */
        void BeginFrame(VkCommandBuffer Commands, std::uint32_t SlotIndex);

        /** Writes the frame-end timestamp and marks the slot's results as pending. */
        void EndFrame(VkCommandBuffer Commands);

        /** Returns a token for EndPass (0xFFFFFFFF when out of queries or unsupported). */
        [[nodiscard]] std::uint32_t BeginPass(VkCommandBuffer Commands, std::string_view Name);

        /** Writes the pass-end timestamp for the token returned by BeginPass (no-op for invalid tokens). */
        void                        EndPass(VkCommandBuffer Commands, std::uint32_t Token);

        /** Per-pass timings of the most recent collected frame. */
        [[nodiscard]] std::span<const PassTiming> Results() const noexcept { return m_Results; }

        /** Whole-frame GPU timing. */
        [[nodiscard]] const PassTiming&           Frame() const noexcept { return m_Frame; }

    private:
        /** Reads back the slot's pending queries and updates the timing averages. */
        void Collect(Slot& Target);
    };
}
