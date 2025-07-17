// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules//CommandPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Renderer.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include <iterator>

void luvk::CommandPool::CreateCommandPool(std::shared_ptr<IRenderModule> const& DeviceModule, std::uint32_t const QueueFamilyIndex, VkCommandPoolCreateFlags const Flags)
{
    m_DeviceModule = DeviceModule;
    auto const CastDevice = std::dynamic_pointer_cast<luvk::Device>(DeviceModule);
    VkDevice const& LogicalDevice = CastDevice->GetLogicalDevice();

    VkCommandPoolCreateInfo const Info{.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .pNext = nullptr, .flags = Flags, .queueFamilyIndex = QueueFamilyIndex};

    if (!LUVK_EXECUTE(vkCreateCommandPool(LogicalDevice, &Info, nullptr, &m_CommandPool)))
    {
        throw std::runtime_error("Failed to create command pool.");
    }

    GetEventSystem().Execute(CommandPoolEvents::OnCreatedPool);
}

std::vector<VkCommandBuffer> luvk::CommandPool::AllocateBuffers(VkDevice const& LogicalDevice, std::uint32_t const Count, VkCommandBufferLevel const Level)
{
    VkCommandBufferAllocateInfo const AllocateInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                   .commandPool = m_CommandPool,
                                                   .level = Level,
                                                   .commandBufferCount = Count};

    std::vector<VkCommandBuffer> Buffers(Count);
    if (!LUVK_EXECUTE(vkAllocateCommandBuffers(LogicalDevice, &AllocateInfo, std::data(Buffers))))
    {
        throw std::runtime_error("Failed to allocate command buffers.");
    }

    m_Buffers.insert(std::end(m_Buffers), std::begin(Buffers), std::end(Buffers));
    return Buffers;
}

void luvk::CommandPool::InitializeDependencies(std::shared_ptr<IRenderModule> const&)
{
    // Do nothing
}

void luvk::CommandPool::ClearResources()
{
    if (!m_DeviceModule)
    {
        return;
    }
    auto const DeviceModule = std::dynamic_pointer_cast<luvk::Device>(m_DeviceModule);
    VkDevice const& LogicalDevice = DeviceModule->GetLogicalDevice();

    if (!std::empty(m_Buffers))
    {
        vkFreeCommandBuffers(LogicalDevice, m_CommandPool, static_cast<std::uint32_t>(std::size(m_Buffers)), std::data(m_Buffers));
        m_Buffers.clear();
    }

    if (m_CommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(LogicalDevice, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
        GetEventSystem().Execute(CommandPoolEvents::OnDestroyedPool);
    }
}
