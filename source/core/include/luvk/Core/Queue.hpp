// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <span>
#include "luvk/Core/Common.hpp"

struct VkCommandBufferSubmitInfo;
struct VkSemaphoreSubmitInfo;
struct VkPresentInfoKHR;

namespace luvk
{
    /** Role of a device queue (see Context::GetQueueInfo). */
    enum class QueueRole : std::uint8_t
    {
        /** Graphics/compute queue: frames, presentation, acceleration structure builds. */
        Graphics,

        /** Transfer queue of the UploadManager (a dedicated DMA queue when the device has one). */
        Transfer,

        /** Async compute queue (a family with compute and without graphics), or the graphics queue without one. */
        Compute,

        /** Extra present-capable queue for third parties (ContextDesc::ExtraQueues.Present); never used by luvk. */
        Present,

        /** Extra queue for third parties (ContextDesc::ExtraQueues.ImageAcquire, e.g. FidelityFX's imageAcquireQueue). */
        ImageAcquire,
    };

    /** Raw description of a device queue, e.g. to hand it to a third-party library. */
    struct QueueInfo
    {
        /** Vulkan queue handle. */
        VkQueue Queue = nullptr;

        /** Queue family index. */
        std::uint32_t Family = 0U;

        /** Queue index within the family. */
        std::uint32_t Index = 0U;

        /**
         * True when luvk itself submits to this VkQueue. Third-party code must then only touch it through
         * Queue::WithLock of the matching luvk::Queue (Context::GraphicsQueue/TransferQueue/ComputeQueue).
         */
        bool UsedByLuvk = true;
    };

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
        VkDevice m_Device = nullptr;

        /** Wrapped VkQueue handle; submissions and presentation are serialised through m_Mutex. */
        VkQueue m_Queue = nullptr;

        /** Timeline semaphore signalled by every Submit() with the value it returns. */
        VkSemaphore m_Timeline = nullptr;

        /** Queue family index of the wrapped queue. */
        std::uint32_t m_Family = 0U;

        /** Queue index within m_Family. */
        std::uint32_t m_Index = 0U;

        /** Device-lost flag of the owning Context: once set, waits return immediately (the work is treated as done). */
        const std::atomic<bool>* m_DeviceLost = nullptr;

        /**
         * Mutex serialising access to m_Queue (shared with the other Queue when both wrap one VkQueue). Recursive, so code
         * running under the lock (e.g. a SwapchainProvider's present replacement) may submit through WithLock again.
         */
        std::recursive_mutex* m_Mutex = nullptr;

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

        /**
         * Binds the device, queue handle, family/queue index and shared mutex, then creates the timeline semaphore.
         * DeviceLost (optional) is the owning Context's device-lost flag: once it is set, Wait/WaitIdle return at once.
         */
        void Initialize(VkDevice Device, VkQueue Handle, std::uint32_t Family, std::recursive_mutex& Mutex, std::uint32_t Index = 0U,
                        const std::atomic<bool>* DeviceLost = nullptr);

        /** Destroys the timeline semaphore and clears the queue handle. Safe to call twice. */
        void Destroy() noexcept;

        /** Submits and signals the queue timeline. Returns the signalled timeline value. */
        std::uint64_t Submit(std::span<const VkCommandBufferSubmitInfo> CommandBuffers,
                             std::span<const VkSemaphoreSubmitInfo>     Waits   = {},
                             std::span<const VkSemaphoreSubmitInfo>     Signals = {});

        /** Presents with Info while holding the queue lock. Returns vkQueuePresentKHR's VkResult. */
        [[nodiscard]] std::int32_t Present(const VkPresentInfoKHR& Info);

        /** Blocks until every submission on this queue has completed (no-op once the device is lost). */
        void WaitIdle();

        /**
         * Runs Function(VkQueue) while holding the queue lock. Use it for third-party code that submits on
         * this queue directly (e.g. ImGui texture uploads, FidelityFX's VkQueueInfoFFXAPI::submitFunc) so it never races
         * with luvk submissions. The lock is recursive: calling it from inside a luvk present is fine.
         */
        template <typename Function>
        decltype(auto) WithLock(Function&& Func)
        {
            std::scoped_lock Lock(*m_Mutex);
            return std::forward<Function>(Func)(m_Queue);
        }

        /**
         * Blocks until the timeline reaches Value. Returns false on timeout. The wait is sliced, so a device loss
         * reported by another thread ends it; once the device is lost every value counts as reached (returns true).
         */
        bool Wait(std::uint64_t Value, std::uint64_t TimeoutNs = UINT64_MAX) const;

        /** Highest timeline value the GPU has signalled so far (every submitted value once the device is lost). */
        [[nodiscard]] std::uint64_t CompletedValue() const;

        /** True once the owning Context observed VK_ERROR_DEVICE_LOST. */
        [[nodiscard]] bool IsDeviceLost() const noexcept
        {
            return m_DeviceLost != nullptr && m_DeviceLost->load(std::memory_order_acquire);
        }

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

        /** Queue index within the family. */
        [[nodiscard]] std::uint32_t Index() const noexcept
        {
            return m_Index;
        }

        /** Timeline semaphore handle. */
        [[nodiscard]] VkSemaphore Timeline() const noexcept
        {
            return m_Timeline;
        }
    };
}
