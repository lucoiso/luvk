// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/CommandBufferPool.hpp"
#include <iterator>
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

CommandBufferPool::~CommandBufferPool()
{
    Destroy();
}

void CommandBufferPool::Create(const std::shared_ptr<Device>& DeviceModule, const std::uint32_t QueueFamilyIndex, const VkCommandPoolCreateFlags Flags)
{
    m_DeviceModule = DeviceModule;
    const VkDevice& DeviceHandle = m_DeviceModule->GetLogicalDevice();

    const VkCommandPoolCreateInfo Info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = nullptr, .flags = Flags, .queueFamilyIndex = QueueFamilyIndex};
    if (!LUVK_EXECUTE(vkCreateCommandPool(DeviceHandle, &Info, nullptr, &m_Pool)))
    {
        throw std::runtime_error("Failed to create command pool");
    }
}

void CommandBufferPool::Destroy()
{
    if (m_Pool != VK_NULL_HANDLE && m_DeviceModule)
    {
        const auto& DevicePtr = m_DeviceModule;
        const VkDevice& DeviceHandle = DevicePtr->GetLogicalDevice();

        if (!std::empty(m_Buffers))
        {
            vkFreeCommandBuffers(DeviceHandle, m_Pool, static_cast<std::uint32_t>(std::size(m_Buffers)), std::data(m_Buffers));
            m_Buffers.clear();
        }

        vkDestroyCommandPool(DeviceHandle, m_Pool, nullptr);
        m_Pool = VK_NULL_HANDLE;
    }
    m_DeviceModule.reset();
    m_Free.clear();
}

VkCommandBuffer CommandBufferPool::Acquire()
{
    std::lock_guard lock(m_Mutex);
    if (!m_DeviceModule || m_Pool == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }

    if (!std::empty(m_Free))
    {
        const VkCommandBuffer CommandBuffer = m_Free.back();
        m_Free.pop_back();
        return CommandBuffer;
    }

    const auto& DevicePtr = m_DeviceModule;
    const VkDevice& DeviceHandle = DevicePtr->GetLogicalDevice();

    const VkCommandBufferAllocateInfo AllocInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                .commandPool = m_Pool,
                                                .level = VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                                                .commandBufferCount = 1};

    VkCommandBuffer CommandBuffer{};
    if (!LUVK_EXECUTE(vkAllocateCommandBuffers(DeviceHandle, &AllocInfo, &CommandBuffer)))
    {
        throw std::runtime_error("Failed to allocate command buffer");
    }
    m_Buffers.push_back(CommandBuffer);
    return CommandBuffer;
}

void CommandBufferPool::Reset()
{
    if (!m_DeviceModule || m_Pool == VK_NULL_HANDLE)
    {
        return;
    }

    vkResetCommandPool(m_DeviceModule->GetLogicalDevice(), m_Pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    m_Free = m_Buffers;
}
