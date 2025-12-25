// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <span>
#include <volk.h>
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Interfaces/IEventModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Device;

    enum class CommandPoolEvents : std::uint8_t
    {
        OnCreatedPool,
        OnAllocatedBuffers,
        OnDestroyedPool
    };

    class LUVK_API CommandPool : public IRenderModule,
                                 public IEventModule
    {
    protected:
        VkCommandPool                m_CommandPool{VK_NULL_HANDLE};
        std::array<VkCommandBuffer, Constants::ImageCount> m_Buffers{};
        std::shared_ptr<Device>      m_DeviceModule{};

    public:
        CommandPool() = delete;
        explicit CommandPool(const std::shared_ptr<Device>& DeviceModule);

        ~CommandPool() override
        {
            CommandPool::ClearResources();
        }

        void CreateCommandPool(std::uint32_t            QueueFamilyIndex,
                               VkCommandPoolCreateFlags Flags);

        void AllocateBuffers(VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        [[nodiscard]] constexpr std::span<const VkCommandBuffer> GetBuffers() const noexcept
        {
            return m_Buffers;
        }

        [[nodiscard]] constexpr VkCommandPool GetCommandPool() const noexcept
        {
            return m_CommandPool;
        }

    protected:
        void ClearResources() override;
    };
} // namespace luvk
