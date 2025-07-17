// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include <vector>
#include <string_view>

#include <volk/volk.h>

namespace luvk
{
    /** Command pool event keys */
    enum class CommandPoolEvents : std::uint8_t
    {
        OnCreatedPool, OnDestroyedPool
    };

    /** Render module responsible for command pool management */
    class LUVKMODULE_API CommandPool : public IRenderModule
    {
        /** Handle to the Vulkan command pool */
        VkCommandPool m_CommandPool{VK_NULL_HANDLE};

        /** Buffers allocated from this pool */
        std::vector<VkCommandBuffer> m_Buffers{};

        /** Device module used for pool creation */
        std::shared_ptr<IRenderModule> m_DeviceModule{};

    public:
        constexpr CommandPool() = default;

        ~CommandPool() override
        {
            CommandPool::ClearResources();
        }

        /** Create the command pool */
        void CreateCommandPool(std::shared_ptr<IRenderModule> const& DeviceModule, std::uint32_t QueueFamilyIndex, VkCommandPoolCreateFlags Flags);

        /** Allocate command buffers from the pool */
        [[nodiscard]] std::vector<VkCommandBuffer> AllocateBuffers(VkDevice const& LogicalDevice,
                                                                   std::uint32_t Count,
                                                                   VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        /** Retrieve the underlying Vulkan command pool */
        [[nodiscard]] constexpr VkCommandPool const& GetCommandPool() const
        {
            return m_CommandPool;
        }

    private:
    
        /** Retrieve dependencies after renderer initialization */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override;

        /** Destroy the Vulkan command pool */
        void ClearResources() override;
    };
} // namespace luvk
