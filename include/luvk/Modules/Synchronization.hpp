/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <array>
#include <volk.h>
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Interfaces/IModule.hpp"

namespace luvk
{
    /**
     * Data structure holding all synchronization primitives for a single frame-in-flight.
     */
    struct LUVK_API FrameData
    {
        /** Command buffer recorded for this frame. */
        VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};

        /** Semaphore signaled when the swap chain image is ready. */
        VkSemaphore ImageAvailable{VK_NULL_HANDLE};

        /** Semaphore signaled when rendering is complete for this frame. */
        VkSemaphore RenderFinished{VK_NULL_HANDLE};

        /** Fence signaled when the GPU finishes executing commands for this frame. */
        VkFence InFlight{VK_NULL_HANDLE};
    };

    /**
     * Module managing per-frame synchronization objects (semaphores and fences).
     */
    class LUVK_API Synchronization : public IModule
    {
        /** Array of FrameData, sized by MaxFramesInFlight. */
        std::array<FrameData, Constants::MaxFramesInFlight> m_Frames{};

        /** Index of the frame currently being recorded. */
        std::uint32_t m_CurrentFrame{0U};

        /** Actual number of frames in flight (e.g., 2 for double buffering). */
        std::uint32_t m_FrameCount{2U};

        /** Pointer to the central service locator. */
        IServiceLocator* m_ServiceLocator{nullptr};

    public:
        /** Default destructor. */
        ~Synchronization() override = default;

        /** Called upon module initialization (creates synchronization objects). */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /** Called upon module shutdown (destroys synchronization objects). */
        void OnShutdown() override;

        /**
         * Get the FrameData object for the current frame index.
         * @return Reference to the current FrameData.
         */
        [[nodiscard]] FrameData& GetCurrentFrame() noexcept
        {
            return m_Frames[m_CurrentFrame];
        }

        /** Get the index of the current frame in the array. */
        [[nodiscard]] std::uint32_t GetCurrentFrameIndex() const noexcept
        {
            return m_CurrentFrame;
        }

        /** Moves the current frame index to the next frame in flight. */
        void AdvanceFrame();

        /**
         * Waits for the current frame's fence to be signaled.
         */
        void WaitForCurrentFrame() const;
    };
}
