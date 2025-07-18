// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <mutex>
#include <vector>
#include <volk/volk.h>
#include "luvk/Module.hpp"

namespace luvk
{
    class Device;

    class LUVKMODULE_API CommandBufferPool
    {
        VkCommandPool m_Pool{VK_NULL_HANDLE};
        std::vector<VkCommandBuffer> m_Buffers{};
        std::vector<VkCommandBuffer> m_Free{};
        std::shared_ptr<Device> m_Device{};
        std::mutex m_Mutex{};

    public:
        constexpr CommandBufferPool() = default;
        ~CommandBufferPool();

        void Create(std::shared_ptr<Device> const& DeviceModule, std::uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags Flags);
        void Destroy();
        VkCommandBuffer Acquire();
        void Reset();
    };
} // namespace luvk
