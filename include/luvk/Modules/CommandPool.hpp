/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <array>
#include <span>
#include <vector>
#include <volk.h>
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Device;

    class LUVK_API CommandPool : public IRenderModule
    {
    protected:
        VkCommandPool                m_CommandPool{VK_NULL_HANDLE};
        std::vector<VkCommandBuffer> m_Buffers{};
        std::shared_ptr<Device>      m_DeviceModule{};

    public:
        CommandPool() = delete;
        explicit CommandPool(const std::shared_ptr<Device>& DeviceModule);

        ~CommandPool() override
        {
            CommandPool::ClearResources();
        }

        void                                               CreateCommandPool(std::uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags Flags);
        std::vector<VkCommandBuffer>                       AllocateBuffers(std::uint32_t Count, VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        std::array<VkCommandBuffer, Constants::ImageCount> AllocateRenderCommandBuffers(VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

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
}
