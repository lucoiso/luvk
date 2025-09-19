// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <mutex>
#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class Device;

    class LUVKMODULE_API CommandBufferPool
    {
        VkCommandPool m_Pool{VK_NULL_HANDLE};
        Vector<VkCommandBuffer> m_Buffers{};
        Vector<VkCommandBuffer> m_Free{};
        std::shared_ptr<Device> m_DeviceModule{};
        std::mutex m_Mutex{};

    public:
        constexpr CommandBufferPool() = default;
        ~CommandBufferPool();

        void Create(const std::shared_ptr<Device>& DeviceModule, std::uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags Flags);
        void Destroy();
        VkCommandBuffer Acquire();
        void Reset();
    };
} // namespace luvk
