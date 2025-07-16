// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Modules/Module.hpp"
#include <volk/volk.h>
#include <memory>
#include <vector>
#include <mutex>

namespace luvk
{
    class Device;

    class LUVKMODULE_API CommandBufferPool
    {
        /** Handle to the created command pool */
        VkCommandPool m_Pool{VK_NULL_HANDLE};

        /** Command buffers allocated from the pool */
        std::vector<VkCommandBuffer> m_Buffers{};

        /** Pool of unused command buffers ready to be recycled */
        std::vector<VkCommandBuffer> m_Free{};

        /** Device used to create the pool */
        std::shared_ptr<Device> m_Device{};

        /** Synchronization primitive for multi-threading */
        std::mutex m_Mutex{};

    public: /** Default constructor */
        constexpr CommandBufferPool() = default;

        /** Destructor */
        ~CommandBufferPool();

        /** Create the command pool and allocate internal resources */
        void Create(std::shared_ptr<Device> const& DeviceModule, std::uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags Flags);

        /** Release the allocated command pool and buffers */
        void Destroy();

        /** Acquire a primary command buffer from the pool */
        VkCommandBuffer Acquire();

        /** Reset the pool freeing command buffers for reuse */
        void Reset();
    };
} // namespace luvk
