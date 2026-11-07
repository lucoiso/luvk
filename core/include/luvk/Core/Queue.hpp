// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <span>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    /**
     * A device queue paired with its own timeline semaphore.
     *
     * Every Submit() signals the next timeline value, so "has this work finished?" is a single integer
     * comparison. Submission and presentation are serialised with a mutex (VkQueue access must be
     * externally synchronised); two Queue objects may wrap the same VkQueue and then share that mutex.
     */
    class LUVK_API Queue
    {
        /** Logical device used for timeline semaphore operations. */
        VkDevice m_Device = VK_NULL_HANDLE;

        /** Wrapped VkQueue handle; submissions and presentation are serialised through m_Mutex. */
        VkQueue m_Queue = VK_NULL_HANDLE;

        /** Timeline semaphore signalled by every Submit() with the value it returns. */
        VkSemaphore m_Timeline = VK_NULL_HANDLE;

        /** Queue family index of the wrapped queue. */
        std::uint32_t m_Family = 0U;

        /** Mutex serialising access to m_Queue (shared with the other Queue when both wrap one VkQueue). */
        std::mutex* m_Mutex = nullptr;

        /** Timeline value that the next Submit() will signal (starts at 1). */
        std::atomic<std::uint64_t> m_NextValue = 1U;

    public:
        /** Constructor. */
        Queue() = default;

        /** Destructor. */
        ~Queue();

        /** Deleted copy constructor. */
        Queue(const Queue&)            = delete;

        /** Deleted copy assignment. */
        Queue& operator=(const Queue&) = delete;

        /** Deleted move constructor. */
        Queue(Queue&&)                 = delete;

        /** Deleted move assignment. */
        Queue& operator=(Queue&&)      = delete;

        /** Binds the device, queue handle, family index and shared mutex, then creates the timeline semaphore. */
        void Initialize(VkDevice Device, VkQueue Handle, std::uint32_t Family, std::mutex& Mutex);

        /** Destroys the timeline semaphore and clears the queue handle. Safe to call twice. */
        void Destroy() noexcept;

        /** Submits and signals the queue timeline. Returns the signalled timeline value. */
        std::uint64_t Submit(std::span<const VkCommandBufferSubmitInfo> CommandBuffers,
                             std::span<const VkSemaphoreSubmitInfo>     Waits   = {},
                             std::span<const VkSemaphoreSubmitInfo>     Signals = {});

        /** Presents with Info while holding the queue lock. Returns vkQueuePresentKHR's result. */
        [[nodiscard]] VkResult Present(const VkPresentInfoKHR& Info);

        /** Blocks until every submission on this queue has completed (vkQueueWaitIdle). */
        void WaitIdle();

        /**
         * Runs Function(VkQueue) while holding the queue lock. Use it for third-party code that submits on
         * this queue directly (e.g. ImGui texture uploads) so it never races with luvk submissions.
         */
        template <typename Function>
        decltype(auto) WithLock(Function&& Func)
        {
            std::scoped_lock Lock(*m_Mutex);
            return std::forward<Function>(Func)(m_Queue);
        }

        /** Blocks until the timeline reaches Value. Returns false on timeout. */
        bool Wait(std::uint64_t Value, std::uint64_t TimeoutNs = UINT64_MAX) const;

        /** Highest timeline value the GPU has signalled so far (vkGetSemaphoreCounterValue). */
        [[nodiscard]] std::uint64_t CompletedValue() const;

        /** Value that the next Submit() will signal: anything retired now is safe once this value is reached. */
        [[nodiscard]] std::uint64_t NextValue() const noexcept
        {
            return m_NextValue.load(std::memory_order_acquire);
        }

        /** Timeline value of the most recent submission (0 when nothing was submitted yet). */
        [[nodiscard]] std::uint64_t LastSubmittedValue() const noexcept
        {
            return NextValue() - 1U;
        }

        /** Raw VkQueue handle (use WithLock for direct submissions). */
        [[nodiscard]] VkQueue Handle() const noexcept
        {
            return m_Queue;
        }

        /** Queue family index. */
        [[nodiscard]] std::uint32_t Family() const noexcept
        {
            return m_Family;
        }

        /** Timeline semaphore handle. */
        [[nodiscard]] VkSemaphore Timeline() const noexcept
        {
            return m_Timeline;
        }
    };
}
