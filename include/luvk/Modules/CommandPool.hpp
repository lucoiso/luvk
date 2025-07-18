// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Types/Vector.hpp"
#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

namespace luvk
{
    enum class CommandPoolEvents : std::uint8_t
    {
        OnCreatedPool, OnDestroyedPool
    };

    class LUVKMODULE_API CommandPool : public IRenderModule
    {
        VkCommandPool m_CommandPool{VK_NULL_HANDLE};
        luvk::Vector<VkCommandBuffer> m_Buffers{};
        std::shared_ptr<IRenderModule> m_DeviceModule{};

    public:
        constexpr CommandPool() = default;

        ~CommandPool() override
        {
            CommandPool::ClearResources();
        }

        void CreateCommandPool(std::shared_ptr<IRenderModule> const& DeviceModule,
                               std::uint32_t QueueFamilyIndex,
                               VkCommandPoolCreateFlags Flags);

        [[nodiscard]] luvk::Vector<VkCommandBuffer> AllocateBuffers(VkDevice const& LogicalDevice,
                                                                   std::uint32_t Count,
                                                                   VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        [[nodiscard]] constexpr VkCommandPool const& GetCommandPool() const
        {
            return m_CommandPool;
        }

    private:
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override;
        void ClearResources() override;
    };
} // namespace luvk
