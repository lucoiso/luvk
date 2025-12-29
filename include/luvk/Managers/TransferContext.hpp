/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <functional>
#include <mutex>
#include <volk.h>

namespace luvk
{
    /**
     * Manages immediate context submission for resource uploads.
     * Uses Vulkan 1.4 Synchronization2.
     */
    class LUVK_API TransferContext
    {
        /** Logical device handle. */
        VkDevice m_Device{VK_NULL_HANDLE};

        /** Queue for submitting transfer commands. */
        VkQueue m_Queue{VK_NULL_HANDLE};

        /** Command pool for allocating temporary command buffers. */
        VkCommandPool m_CommandPool{VK_NULL_HANDLE};

        /** Mutex to synchronize access to immediate submission. */
        std::mutex m_Mutex{};

    public:
        /** Default constructor. */
        constexpr TransferContext() = default;

        /** Destructor. */
        ~TransferContext();

        /**
         * Initializes the transfer context with the necessary Vulkan objects.
         * @param Device Logical device.
         * @param Queue Queue for command submission.
         * @param QueueFamilyIndex Index of the queue family.
         */
        void Initialize(VkDevice Device, VkQueue Queue, std::uint32_t QueueFamilyIndex);

        /** Cleans up the Vulkan resources. */
        void Shutdown();

        /**
         * Submit a command immediately and wait for idle.
         * Note: Blocking operation. Use carefully.
         * @param Recorder The function to record commands into the transient command buffer.
         */
        void SubmitImmediate(std::function<void(VkCommandBuffer)>&& Recorder);
    };
}
