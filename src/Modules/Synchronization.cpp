/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Synchronization.hpp"
#include <stdexcept>
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/CommandPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/SwapChain.hpp"

luvk::Synchronization::Synchronization(const std::shared_ptr<Device>& DeviceModule)
    : m_DeviceModule(DeviceModule) {}

void luvk::Synchronization::Initialize(std::span<const VkCommandBuffer, Constants::ImageCount> CommandBuffers)
{
    const VkDevice LogicalDevice = m_DeviceModule->GetLogicalDevice();

    constexpr VkSemaphoreCreateInfo SemInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    constexpr VkFenceCreateInfo     FenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};

    for (std::size_t Index = 0; Index < Constants::ImageCount; ++Index)
    {
        FrameData& Frame    = m_Frames.at(Index);
        Frame.CommandBuffer = CommandBuffers[Index];

        if (!LUVK_EXECUTE(vkCreateSemaphore(LogicalDevice, &SemInfo, nullptr, &Frame.ImageAvailable)))
        {
            throw std::runtime_error("Failed to create frame image semaphore.");
        }

        if (!LUVK_EXECUTE(vkCreateSemaphore(LogicalDevice, &SemInfo, nullptr, &m_RenderFinished[Index])))
        {
            throw std::runtime_error("Failed to create frame render semaphore.");
        }

        if (!LUVK_EXECUTE(vkCreateFence(LogicalDevice, &FenceInfo, nullptr, &Frame.InFlight)))
        {
            throw std::runtime_error("Failed to create frame fence.");
        }
    }

    m_CurrentFrame = 0;
}

void luvk::Synchronization::ResetFrames()
{
    const VkDevice LogicalDevice = m_DeviceModule->GetLogicalDevice();

    constexpr VkSemaphoreCreateInfo SemInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    for (std::size_t Index = 0; Index < Constants::ImageCount; ++Index)
    {
        FrameData& FrameIt = m_Frames.at(Index);
        FrameIt.SecondaryBuffers.clear();

        if (FrameIt.ImageAvailable != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(LogicalDevice, FrameIt.ImageAvailable, nullptr);
            FrameIt.ImageAvailable = VK_NULL_HANDLE;
        }

        if (!LUVK_EXECUTE(vkCreateSemaphore(LogicalDevice, &SemInfo, nullptr, &FrameIt.ImageAvailable)))
        {
            throw std::runtime_error("Failed to create semaphore");
        }

        if (FrameIt.InFlight != VK_NULL_HANDLE)
        {
            vkResetFences(LogicalDevice, 1, &FrameIt.InFlight);
        }

        FrameIt.Submitted = false;
    }

    for (VkSemaphore& SemaphoreIt : m_RenderFinished)
    {
        vkDestroySemaphore(LogicalDevice, SemaphoreIt, nullptr);
        if (!LUVK_EXECUTE(vkCreateSemaphore(LogicalDevice, &SemInfo, nullptr, &SemaphoreIt)))
        {
            throw std::runtime_error("Failed to create semaphore.");
        }
    }
}

void luvk::Synchronization::WaitFrame(const FrameData& Frame, const VkBool32 WaitAll, const std::uint64_t Timeout) const
{
    if (Frame.Submitted && Frame.InFlight != VK_NULL_HANDLE)
    {
        vkWaitForFences(m_DeviceModule->GetLogicalDevice(), 1, &Frame.InFlight, WaitAll, Timeout);
    }
}

void luvk::Synchronization::WaitFrame(const std::size_t Index, const VkBool32 WaitAll, const std::uint64_t Timeout) const
{
    WaitFrame(m_Frames.at(Index), WaitAll, Timeout);
}

void luvk::Synchronization::WaitCurrentFrame(const VkBool32 WaitAll, const std::uint64_t Timeout) const
{
    WaitFrame(m_CurrentFrame, WaitAll, Timeout);
}

void luvk::Synchronization::ClearResources()
{
    const VkDevice LogicalDevice = m_DeviceModule->GetLogicalDevice();

    for (FrameData& FrameIt : m_Frames)
    {
        FrameIt.CommandBuffer = VK_NULL_HANDLE;

        if (FrameIt.ImageAvailable != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(LogicalDevice, FrameIt.ImageAvailable, nullptr);
            FrameIt.ImageAvailable = VK_NULL_HANDLE;
        }

        if (FrameIt.InFlight != VK_NULL_HANDLE)
        {
            vkDestroyFence(LogicalDevice, FrameIt.InFlight, nullptr);
            FrameIt.InFlight = VK_NULL_HANDLE;
        }

        FrameIt.Submitted = false;
    }

    for (VkSemaphore& SemaphoreIt : m_RenderFinished)
    {
        vkDestroySemaphore(LogicalDevice, SemaphoreIt, nullptr);
        SemaphoreIt = VK_NULL_HANDLE;
    }
}
