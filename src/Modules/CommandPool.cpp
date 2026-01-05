/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/CommandPool.hpp"
#include <stdexcept>
#include "luvk/Interfaces/IServiceLocator.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

void CommandPool::OnInitialize(IServiceLocator* ServiceLocator)
{
    m_ServiceLocator = ServiceLocator;
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();

    const auto GraphicsFamily = DeviceMod->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    if (!GraphicsFamily.has_value())
    {
        throw std::runtime_error("No graphics queue family found");
    }

    const VkCommandPoolCreateInfo Info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                       .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                       .queueFamilyIndex = GraphicsFamily.value()};

    if (!LUVK_EXECUTE(vkCreateCommandPool(DeviceMod->GetLogicalDevice(), &Info, nullptr, &m_Pool)))
    {
        throw std::runtime_error("Failed to create command pool");
    }

    IModule::OnInitialize(ServiceLocator);
}

void CommandPool::OnShutdown()
{
    if (m_Pool != VK_NULL_HANDLE)
    {
        const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
        vkDestroyCommandPool(DeviceMod->GetLogicalDevice(), m_Pool, nullptr);
        m_Pool = VK_NULL_HANDLE;
    }

    IModule::OnShutdown();
}

std::vector<VkCommandBuffer> CommandPool::Allocate(const std::uint32_t Count, const VkCommandBufferLevel Level)
{
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    std::vector<VkCommandBuffer> Buffers(Count);

    const VkCommandBufferAllocateInfo Info{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                           .commandPool = m_Pool,
                                           .level = Level,
                                           .commandBufferCount = Count};

    if (!LUVK_EXECUTE(vkAllocateCommandBuffers(DeviceMod->GetLogicalDevice(), &Info, std::data(Buffers))))
    {
        throw std::runtime_error("Failed to allocate command buffers");
    }

    m_Buffers.insert(m_Buffers.end(), Buffers.begin(), Buffers.end());

    return Buffers;
}

void CommandPool::Free(const std::span<const VkCommandBuffer> Buffers) const
{
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    vkFreeCommandBuffers(DeviceMod->GetLogicalDevice(), m_Pool, static_cast<std::uint32_t>(std::size(Buffers)), std::data(Buffers));
}

void CommandPool::Reset(const bool ReleaseResources) const
{
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    vkResetCommandPool(DeviceMod->GetLogicalDevice(), m_Pool, ReleaseResources ? VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT : 0);
}
