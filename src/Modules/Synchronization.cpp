// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Synchronization.hpp"
#include <iterator>
#include <stdexcept>
#include <thread>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules//CommandPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Renderer.hpp"
#include "luvk/Modules/SwapChain.hpp"

void luvk::Synchronization::Initialize(const std::shared_ptr<Device>& DeviceModule, const std::size_t FrameCount)
{
    m_DeviceModule = DeviceModule;
    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

    constexpr VkSemaphoreCreateInfo SemInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    constexpr VkFenceCreateInfo FenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    m_Frames.resize(FrameCount);
    m_RenderFinished.resize(FrameCount, VK_NULL_HANDLE);

    for (std::size_t Index = 0; Index < FrameCount; ++Index)
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

void luvk::Synchronization::SetupFrames(const std::shared_ptr<SwapChain>& SwapChainModule,
                                        const std::shared_ptr<CommandPool>& CommandPoolModule)
{
    m_ThreadCount = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    m_DeviceModule->WaitIdle();

    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();
    const std::size_t ImageCount = std::size(SwapChainModule->GetImageViews());

    const Vector<VkCommandBuffer> Buffers = CommandPoolModule->AllocateBuffers(static_cast<std::uint32_t>(ImageCount));
    const std::uint32_t GraphicsFamily = m_DeviceModule->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value();

    m_SecondaryPool.Destroy();
    m_SecondaryPool.Create(m_DeviceModule, GraphicsFamily, 0);

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

    m_SecondaryPool.Destroy();

    m_Frames.clear();
    m_RenderFinished.clear();
}
