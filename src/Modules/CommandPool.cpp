// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/CommandPool.hpp"
#include <iterator>
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

luvk::CommandPool::CommandPool(const std::shared_ptr<Device>& DeviceModule)
    : m_DeviceModule(DeviceModule) {}

void luvk::CommandPool::CreateCommandPool(const std::uint32_t QueueFamilyIndex, const VkCommandPoolCreateFlags Flags)
{
    const VkCommandPoolCreateInfo Info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                       .pNext = nullptr,
                                       .flags = Flags,
                                       .queueFamilyIndex = QueueFamilyIndex};

    if (!LUVK_EXECUTE(vkCreateCommandPool(m_DeviceModule->GetLogicalDevice(), &Info, nullptr, &m_CommandPool)))
    {
        throw std::runtime_error("Failed to create command pool.");
    }

    GetEventSystem().Execute(CommandPoolEvents::OnCreatedPool);
}

void luvk::CommandPool::AllocateBuffers(const VkCommandBufferLevel Level)
{
    const VkCommandBufferAllocateInfo AllocateInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                   .commandPool = m_CommandPool,
                                                   .level = Level,
                                                   .commandBufferCount = Constants::ImageCount};

    if (!LUVK_EXECUTE(vkAllocateCommandBuffers(m_DeviceModule->GetLogicalDevice(), &AllocateInfo, std::data(m_Buffers))))
    {
        throw std::runtime_error("Failed to allocate command buffers.");
    }

    GetEventSystem().Execute(CommandPoolEvents::OnAllocatedBuffers);
}

void luvk::CommandPool::ClearResources()
{
    const VkDevice LogicalDevice = m_DeviceModule->GetLogicalDevice();

    if (!std::empty(m_Buffers))
    {
        vkFreeCommandBuffers(LogicalDevice, m_CommandPool, Constants::ImageCount, std::data(m_Buffers));

        for (VkCommandBuffer& BufferIt : m_Buffers)
        {
            BufferIt = VK_NULL_HANDLE;
        }
    }

    if (m_CommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(LogicalDevice, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;

        GetEventSystem().Execute(CommandPoolEvents::OnDestroyedPool);
    }
}
