// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <volk.h>

namespace luvk
{
    class Device;

    class LUVK_API CommandBufferPool
    {
    protected:
        std::mutex                   m_Mutex{};
        VkCommandPool                m_Pool{VK_NULL_HANDLE};
        std::vector<VkCommandBuffer> m_Buffers{};
        std::vector<VkCommandBuffer> m_Free{};
        std::shared_ptr<Device>      m_DeviceModule{};

    public:
        CommandBufferPool() = delete;
        explicit CommandBufferPool(const std::shared_ptr<Device>& DeviceModule);

        ~CommandBufferPool();

        void            Create(std::uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags Flags);
        VkCommandBuffer Acquire();
        void            Reset();
    };
} // namespace luvk
