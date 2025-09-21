// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Synchronization.hpp"
#include <iterator>
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules//CommandPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Renderer.hpp"
#include "luvk/Modules/SwapChain.hpp"

luvk::Synchronization::Synchronization(const std::shared_ptr<Device>& DeviceModule,
                                       const std::shared_ptr<SwapChain>& SwapChainModule,
                                       const std::shared_ptr<CommandPool>& CommandPoolModule,
                                       const std::size_t FrameCount)
    : m_FrameCount(FrameCount),
      m_DeviceModule(DeviceModule),
      m_SwapChainModule(SwapChainModule),
      m_CommandPoolModule(CommandPoolModule) {}

void luvk::Synchronization::Initialize()
{
    m_SecondaryPool = std::make_shared<CommandBufferPool>(m_DeviceModule);
    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

    const std::uint32_t GraphicsFamily = m_DeviceModule->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value();
    m_SecondaryPool->Create(GraphicsFamily, 0);

    constexpr VkSemaphoreCreateInfo SemInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    constexpr VkFenceCreateInfo FenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    m_Frames.resize(m_FrameCount);
    m_RenderFinished.resize(m_FrameCount, VK_NULL_HANDLE);

    for (std::size_t Index = 0; Index < m_FrameCount; ++Index)
    {
        FrameData& Frame = m_Frames.at(Index);

        if (!LUVK_EXECUTE(vkCreateSemaphore(LogicalDevice, &SemInfo, nullptr, &Frame.ImageAvailable)) || !
            LUVK_EXECUTE(vkCreateSemaphore(LogicalDevice, &SemInfo, nullptr, &m_RenderFinished[Index])) ||
            !LUVK_EXECUTE(vkCreateFence(LogicalDevice, &FenceInfo, nullptr, &Frame.InFlight)))
        {
            throw std::runtime_error("Failed to create frame synchronization objects.");
        }
    }
    m_CurrentFrame = 0;
}

void luvk::Synchronization::SetupFrames()
{
    m_DeviceModule->WaitIdle();

    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();
    const std::size_t ImageCount = std::size(m_SwapChainModule->GetImageViews());

    const Vector<VkCommandBuffer> Buffers = m_CommandPoolModule->AllocateBuffers(static_cast<std::uint32_t>(ImageCount));

    m_SecondaryPool->Reset();

    for (std::size_t Index = 0; Index < ImageCount; ++Index)
    {
        FrameData& Frame = m_Frames[Index];
        Frame.CommandBuffer = Buffers[Index];
        Frame.SecondaryBuffers.clear();

        if (Frame.InFlight != VK_NULL_HANDLE)
        {
            vkResetFences(LogicalDevice, 1, &Frame.InFlight);
        }
    }
}

void luvk::Synchronization::ClearResources()
{
    if (!m_DeviceModule)
    {
        return;
    }

    const auto Device = std::dynamic_pointer_cast<luvk::Device>(m_DeviceModule);
    const VkDevice& LogicalDevice = Device->GetLogicalDevice();

    for (FrameData& Frame : m_Frames)
    {
        if (Frame.CommandBuffer != VK_NULL_HANDLE)
        {
            // Command buffers are owned by the command pool
            Frame.CommandBuffer = VK_NULL_HANDLE;
        }
        if (Frame.ImageAvailable != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(LogicalDevice, Frame.ImageAvailable, nullptr);
            Frame.ImageAvailable = VK_NULL_HANDLE;
        }
        if (Frame.InFlight != VK_NULL_HANDLE)
        {
            vkDestroyFence(LogicalDevice, Frame.InFlight, nullptr);
            Frame.InFlight = VK_NULL_HANDLE;
        }
        Frame.SecondaryBuffers.clear();
    }

    for (VkSemaphore& Sem : m_RenderFinished)
    {
        if (Sem != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(LogicalDevice, Sem, nullptr);
            Sem = VK_NULL_HANDLE;
        }
    }

    m_Frames.clear();
    m_RenderFinished.clear();
}
