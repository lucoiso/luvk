/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Managers/TransferContext.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"

using namespace luvk;

TransferContext::~TransferContext()
{
    Shutdown();
}

void TransferContext::Initialize(VkDevice Device, VkQueue Queue, std::uint32_t QueueFamilyIndex)
{
    m_Device = Device;
    m_Queue  = Queue;

    const VkCommandPoolCreateInfo PoolInfo{.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                           .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                           .queueFamilyIndex = QueueFamilyIndex};

    if (!LUVK_EXECUTE(vkCreateCommandPool(m_Device, &PoolInfo, nullptr, &m_CommandPool)))
    {
        throw std::runtime_error("Failed to create transfer command pool");
    }
}

void TransferContext::Shutdown()
{
    if (m_CommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
        m_CommandPool = VK_NULL_HANDLE;
    }
}

void TransferContext::SubmitImmediate(std::function<void(VkCommandBuffer)>&& Recorder)
{
    std::lock_guard Lock(m_Mutex);

    VkCommandBuffer                   CommandBuffer{VK_NULL_HANDLE};
    const VkCommandBufferAllocateInfo AllocInfo{.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                                .commandPool        = m_CommandPool,
                                                .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                .commandBufferCount = 1};

    if (!LUVK_EXECUTE(vkAllocateCommandBuffers(m_Device, &AllocInfo, &CommandBuffer)))
    {
        throw std::runtime_error("Failed to allocate transfer command buffer");
    }

    constexpr VkCommandBufferBeginInfo BeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    vkBeginCommandBuffer(CommandBuffer, &BeginInfo);
    Recorder(CommandBuffer);
    vkEndCommandBuffer(CommandBuffer);

    const VkCommandBufferSubmitInfo CmdInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = CommandBuffer};

    const VkSubmitInfo2 SubmitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2, .commandBufferInfoCount = 1, .pCommandBufferInfos = &CmdInfo};

    vkQueueSubmit2(m_Queue, 1, &SubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Queue);

    vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &CommandBuffer);
}
