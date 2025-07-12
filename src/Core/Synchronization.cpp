// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Synchronization.hpp"
#include "luvk/Core/CommandPool.hpp"
#include "luvk/Core/Device.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/SwapChain.hpp"
#include "luvk/Core/ThreadPool.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include <iterator>
#include <stdexcept>
#include <thread>

void luvk::Synchronization::Initialize(std::shared_ptr<IRenderModule> const& DeviceModule, const std::size_t FrameCount)
{
    m_DeviceModule = DeviceModule;
    auto const Device = std::dynamic_pointer_cast<luvk::Device>(DeviceModule);
    const VkDevice LogicalDevice = Device->GetLogicalDevice();

    constexpr VkSemaphoreCreateInfo SemInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    constexpr VkFenceCreateInfo FenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    m_Frames.resize(FrameCount);
    m_RenderFinished.resize(FrameCount, VK_NULL_HANDLE);

    for (std::size_t Index = 0; Index < FrameCount; ++Index)
    {
        FrameData& Frame = m_Frames[Index];
        if (!LUVK_EXECUTE(vkCreateSemaphore(LogicalDevice, &SemInfo, nullptr, &Frame.ImageAvailable)) || !
            LUVK_EXECUTE(vkCreateSemaphore(LogicalDevice, &SemInfo, nullptr, &m_RenderFinished[Index])) ||
            !LUVK_EXECUTE(vkCreateFence(LogicalDevice, &FenceInfo, nullptr, &Frame.InFlight)))
        {
            throw std::runtime_error("Failed to create frame synchronization objects.");
        }
    }
    m_CurrentFrame = 0;
}

void luvk::Synchronization::SetupFrames(std::shared_ptr<IRenderModule> const& DeviceModule,
                                        std::shared_ptr<IRenderModule> const& SwapChainModule,
                                        std::shared_ptr<IRenderModule> const& CommandPoolModule,
                                        std::shared_ptr<IRenderModule> const& ThreadPoolModule)
{
    const auto Dev = std::dynamic_pointer_cast<luvk::Device>(DeviceModule);
    const auto Pool = std::dynamic_pointer_cast<luvk::CommandPool>(CommandPoolModule);
    const auto Swap = std::dynamic_pointer_cast<luvk::SwapChain>(SwapChainModule);

    m_ThreadCount = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    Dev->WaitIdle();
    const VkDevice LogicalDevice = Dev->GetLogicalDevice();
    const std::size_t ImageCount = std::size(Swap->GetImageViews());

    const std::vector<VkCommandBuffer> Buffers = Pool->AllocateBuffers(LogicalDevice, static_cast<std::uint32_t>(ImageCount));

    m_SecondaryPool.Destroy();
    const std::uint32_t GraphicsFamily = Dev->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value();
    m_SecondaryPool.Create(std::static_pointer_cast<luvk::Device>(DeviceModule), GraphicsFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

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

luvk::Synchronization::FrameData& luvk::Synchronization::GetFrame(const std::size_t Index)
{
    return m_Frames.at(Index);
}

void luvk::Synchronization::AdvanceFrame()
{
    m_CurrentFrame = (m_CurrentFrame + 1) % std::size(m_Frames);
}

void luvk::Synchronization::ClearResources()
{
    if (!m_DeviceModule)
    {
        return;
    }

    const auto Device = std::dynamic_pointer_cast<luvk::Device>(m_DeviceModule);
    const VkDevice LogicalDevice = Device->GetLogicalDevice();

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
