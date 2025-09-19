// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class Device;

    enum class CommandPoolEvents : std::uint8_t
    {
        OnCreatedPool,
        OnDestroyedPool
    };

    class LUVKMODULE_API CommandPool : public IRenderModule
    {
        VkCommandPool m_CommandPool{VK_NULL_HANDLE};
        Vector<VkCommandBuffer> m_Buffers{};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        constexpr CommandPool() = default;

        ~CommandPool() override
        {
            CommandPool::ClearResources();
        }

        void CreateCommandPool(const std::shared_ptr<Device>& DeviceModule,
                               std::uint32_t QueueFamilyIndex,
                               VkCommandPoolCreateFlags Flags);

        [[nodiscard]] Vector<VkCommandBuffer> AllocateBuffers(std::uint32_t Count,
                                                              VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        [[nodiscard]] constexpr const VkCommandPool& GetCommandPool() const
        {
            return m_CommandPool;
        }

    private:
        void InitializeDependencies(const std::shared_ptr<IRenderModule>&) override;
        void ClearResources() override;
    };
} // namespace luvk
