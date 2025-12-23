// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <volk.h>
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
        std::vector<VkCommandBuffer> m_Buffers{};
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

        [[nodiscard]] std::vector<VkCommandBuffer> AllocateBuffers(std::uint32_t        Count,
                                                                   VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        [[nodiscard]] constexpr VkCommandPool GetCommandPool() const noexcept
        {
            return m_CommandPool;
        }

    protected:
        void ClearResources() override;
    };
} // namespace luvk
