// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include "luvk/Core/Common.hpp"
#include "luvk/Render/FrameAllocator.hpp"
#include "luvk/Render/GpuProfiler.hpp"
#include "luvk/Render/PresentTarget.hpp"
#include "luvk/Render/RenderGraph.hpp"
#include "luvk/Render/Swapchain.hpp"

namespace luvk
{
    class Context;

    /** How the async compute submission of a frame is ordered against the graphics work of the previous frames. */
    enum class AsyncComputeOrder : std::uint8_t
    {
        /** The async submission also waits for the previous graphics submission: it may touch anything (default). */
        AfterPreviousFrame,

        /** No wait on the previous graphics work, so async compute overlaps the previous frame on the GPU. The caller
         *  guarantees the async passes do not touch resources (or layouts) the frames in flight still use. */
        Concurrent,
    };

    /** Renderer creation parameters. */
    struct RendererDesc
    {
        /** Main window swapchain. Surface may stay nullptr: the Context's window surface is taken over, and
         *  without any surface (headless Context) the renderer records and submits frames without presenting. */
        SwapchainDesc Swapchain = {};

        /** Independent from the swapchain image count. */
        std::uint32_t FramesInFlight = 2U;

        /** Times every render graph pass (see Renderer::Profiler()). */
        bool GpuTimings = false;
    };

    /** Everything needed to build one frame. Valid until Renderer::EndFrame / Renderer::AbortFrame. */
    struct Frame
    {
        /** Render graph of this frame's slot (already reset for a new frame). */
        RenderGraph&  Graph;

        /**
         * Async compute graph of this frame's slot (GraphQueue::AsyncCompute: PassType::Compute / Transfer passes only).
         * When it has passes, it is recorded and submitted on Context::ComputeQueue() before Graph, and Graph's submission
         * waits for it: async work of a frame runs before the graphics work of the same frame. Share data between both
         * graphs through imported (persistent) resources; the barriers are derived from their tracked state.
         */
        RenderGraph&  AsyncGraph;

        /** Per-frame CPU-written memory, recycled once the GPU is done with this frame. */
        FrameAllocator& Allocator;

        /** Main window swapchain image to render into (invalid for headless renderers). */
        TextureHandle Backbuffer = {};

        /** Main swapchain extent in pixels ({0, 0} for headless renderers). */
        Extent2D Extent = {};

        /** Main swapchain image format (VK_FORMAT_UNDEFINED for headless renderers). */
        luvk::Format Format = luvk::Format::Undefined;

        /** Index of the frame slot in use (0 to FramesInFlight - 1). */
        std::uint32_t FrameSlot = 0U;

        /** Number of this frame (increased when the frame ends). */
        std::uint64_t Number = 0U;
    };

    /**
     * Frame scheduler presenting one or more PresentTargets with a single submission per frame:
     *
     *     if (auto Frame = Renderer.BeginFrame())
     *     {
     *         Frame->Graph.AddPass(...);                         // render into Frame->Backbuffer
     *         if (auto Extra = Renderer.AcquireTarget(Other))    // optional: more windows in the same frame
     *         {
     *             Frame->Graph.AddPass(...);                     // render into Extra->Texture
     *         }
     *         Renderer.EndFrame(*Frame);
     *     }
     *
     * BeginFrame waits for the frame slot on the graphics timeline, collects garbage and acquires the main target
     * (headless renderers, created from a Context without a surface, skip the acquisition and the presentation).
     * EndFrame records the graph, flushes uploads, submits once (waiting on every acquisition and on the uploads)
     * and presents every acquired target. If recording fails, the frame is still closed (every acquired image is
     * transitioned and presented), so the renderer stays usable; AbortFrame does the same on purpose.
     *
     * Async compute: when Frame::AsyncGraph has passes, EndFrame records it first and submits it on the compute queue
     * (waiting on the pending uploads and, with AsyncComputeOrder::AfterPreviousFrame, on the previous graphics
     * submission); the frame's graphics submission then waits for it. After VK_ERROR_DEVICE_LOST the frame is abandoned
     * (nothing is re-submitted) and the error propagates.
     */
    class LUVK_API Renderer
    {
        /** Per frame-in-flight resources. */
        struct FrameSlot
        {
            /** Transient command pool for this slot's command buffer. */
            VkCommandPool Pool = nullptr;

            /** Primary one-shot command buffer recorded every frame. */
            VkCommandBuffer Commands = nullptr;

            /** Graphics timeline value to wait for before reusing this slot. */
            std::uint64_t Value = 0U;

            /** Render graph exclusive to this slot (one graph per frame in flight). */
            std::unique_ptr<RenderGraph> Graph;

            /** Transient command pool of the async compute command buffer (compute queue family). */
            VkCommandPool AsyncPool = nullptr;

            /** Async compute command buffer recorded when the async graph has passes. */
            VkCommandBuffer AsyncCommands = nullptr;

            /** Compute timeline value of the slot's last async submission (0 when none). */
            std::uint64_t AsyncValue = 0U;

            /** Async compute graph exclusive to this slot. */
            std::unique_ptr<RenderGraph> AsyncGraph;
        };

        /** Owning context. */
        Context&                     m_Context;

        /** Main window target (null for headless renderers). */
        std::unique_ptr<PresentTarget> m_MainTarget;

        /** GPU profiler (created when RendererDesc::GpuTimings is set). */
        std::unique_ptr<GpuProfiler> m_Profiler;

        /** GPU profiler of the async compute graphs (created when RendererDesc::GpuTimings is set). */
        std::unique_ptr<GpuProfiler> m_AsyncProfiler;

        /** Ordering of the async compute submissions. */
        AsyncComputeOrder m_AsyncOrder = {AsyncComputeOrder::AfterPreviousFrame};

        /** Per-frame CPU-written memory, one block set per frame slot. */
        FrameAllocator m_Allocator;

        /** One resource set per frame in flight. */
        std::vector<FrameSlot> m_Slots;

        /** Targets acquired by the frame being recorded, with their graph handle. */
        std::vector<std::pair<PresentTarget*, TextureHandle>> m_Acquired;

        /** Number of frames ended so far (drives slot selection). */
        std::uint64_t m_FrameNumber = 0U;

        /** True between BeginFrame and EndFrame / AbortFrame. */
        bool m_FrameActive = false;

    public:
        /** Constructor; creates the frame slots and the main present target. */
        Renderer(Context& Ctx, const RendererDesc& Desc);

        /** Destructor; waits for the graphics timeline and destroys the slots. */
        ~Renderer();

        /** Non-copyable. */
        Renderer(const Renderer&)            = delete;

        /** Non-copyable. */
        Renderer& operator=(const Renderer&) = delete;

        /** Returns std::nullopt when the main target cannot be presented right now (minimised, out of date). */
        [[nodiscard]] std::optional<Frame> BeginFrame();

        /** Acquires another target for the frame being recorded; std::nullopt when it cannot present right now. */
        [[nodiscard]] std::optional<PresentImage> AcquireTarget(PresentTarget& Target);

        /** Records and submits the frame's graph, then presents every acquired target. */
        void EndFrame(const Frame& Current);

        /** Drops the frame's passes but still closes it (acquired images are presented unchanged). */
        void AbortFrame(const Frame& Current);

        /** Requests a new main window size; the swapchain is recreated before the next acquisition. */
        void Resize(Extent2D NewExtent) noexcept;

        /** Ordering of the async compute submissions of the next frames (see AsyncComputeOrder). */
        void SetAsyncComputeOrder(AsyncComputeOrder Order) noexcept { m_AsyncOrder = Order; }

        /** Switches display VSync by recreating the main swapchain. Must be called outside BeginFrame/EndFrame. */
        bool SetVSync(bool Enabled);

        /** Present mode currently selected by the main swapchain. */
        [[nodiscard]] luvk::PresentMode PresentMode() const noexcept;

        /** Ordering of the async compute submissions. */
        [[nodiscard]] AsyncComputeOrder GetAsyncComputeOrder() const noexcept { return m_AsyncOrder; }

        /**
         * Installs (or removes, with nullptr) a SwapchainProvider on the main target: waits for every frame in flight, then
         * recreates the swapchain through the provider. Throws when called during a frame or on a headless renderer.
         */
        void SetSwapchainProvider(std::shared_ptr<SwapchainProvider> Provider);

        /** False for headless renderers (no window to present to). */
        [[nodiscard]] bool HasMainTarget() const noexcept { return m_MainTarget != nullptr; }

        /** Main window target (check HasMainTarget for headless renderers). */
        [[nodiscard]] PresentTarget& MainTarget() noexcept { return *m_MainTarget; }

        /** Main window target (check HasMainTarget for headless renderers). */
        [[nodiscard]] const PresentTarget& MainTarget() const noexcept { return *m_MainTarget; }

        /** GPU profiler, or nullptr when RendererDesc::GpuTimings is disabled. */
        [[nodiscard]] GpuProfiler*  Profiler() const noexcept { return m_Profiler.get(); }

        /** GPU profiler of the async compute graphs (compute queue family), or nullptr when GpuTimings is disabled. */
        [[nodiscard]] GpuProfiler*  AsyncProfiler() const noexcept { return m_AsyncProfiler.get(); }

        /** Number of frame slots (frames in flight). */
        [[nodiscard]] std::uint32_t FramesInFlight() const noexcept { return static_cast<std::uint32_t>(std::size(m_Slots)); }

        /** Number of frames ended so far. */
        [[nodiscard]] std::uint64_t FrameNumber() const noexcept { return m_FrameNumber; }

        /** True between BeginFrame and EndFrame / AbortFrame. */
        [[nodiscard]] bool IsFrameActive() const noexcept { return m_FrameActive; }

    private:
        /** Records the slot's graph (optionally with the profiler), submits once and presents every acquired target. */
        void SubmitAndPresent(const Frame& Current, bool Profile);

        /** Closes a frame whose recording failed: re-records only the acquired images' present transitions. */
        void CloseFrame(const Frame& Current) noexcept;

        /** Drops the frame without submitting anything (device lost or unrecoverable failure). */
        void AbandonFrame() noexcept;

        /** Records the async compute graph of the slot; returns false when it has no passes. */
        [[nodiscard]] bool RecordAsyncGraph(const Frame& Current, bool Profile);

        /** Destroys the per-slot graphs and command pools. */
        void DestroySlots() noexcept;
    };
}
