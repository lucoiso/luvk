// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Interfaces/IEventModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class Device;

    enum class CommandPoolEvents : std::uint8_t
    {
        OnCreatedPool,
        OnDestroyedPool
    };

    class LUVKMODULE_API CommandPool : public IRenderModule,
                                       public IEventModule
    {
        VkCommandPool m_CommandPool{VK_NULL_HANDLE};
        Vector<VkCommandBuffer> m_Buffers{};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        CommandPool() = delete;
        explicit CommandPool(const std::shared_ptr<Device>& DeviceModule);

        ~CommandPool() override
        {
            CommandPool::ClearResources();
        }

        void CreateCommandPool(std::uint32_t QueueFamilyIndex,
                               VkCommandPoolCreateFlags Flags);

        [[nodiscard]] Vector<VkCommandBuffer> AllocateBuffers(std::uint32_t Count,
                                                              VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        [[nodiscard]] constexpr const VkCommandPool& GetCommandPool() const
        {
            return m_CommandPool;
        }

        void ClearResources() override;
    };
} // namespace luvk
