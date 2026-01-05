/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Synchronization.hpp"
#include <stdexcept>
#include "luvk/Interfaces/IServiceLocator.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/CommandPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/SwapChain.hpp"

using namespace luvk;

void Synchronization::OnInitialize(IServiceLocator* ServiceLocator)
{
    m_ServiceLocator = ServiceLocator;
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    auto* CmdPool = m_ServiceLocator->GetModule<CommandPool>();
    const auto* SwapChainMod = m_ServiceLocator->GetModule<SwapChain>();

    if (!DeviceMod || !CmdPool || !SwapChainMod)
    {
        throw std::runtime_error("Dependencies missing for Synchronization");
    }

    const std::uint32_t ImageCount = SwapChainMod->GetImageCount();
    m_FrameCount = ImageCount >= 3 ? 3 : 2;

    const auto Buffers = CmdPool->Allocate(m_FrameCount);

    constexpr VkSemaphoreCreateInfo SemInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    constexpr VkFenceCreateInfo FenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    for (std::uint32_t It = 0; It < m_FrameCount; ++It)
    {
        m_Frames[It].CommandBuffer = Buffers[It];

        if (!LUVK_EXECUTE(vkCreateSemaphore(DeviceMod->GetLogicalDevice(), &SemInfo, nullptr, &m_Frames[It].ImageAvailable)))
        {
            throw std::runtime_error("Failed to create image semaphore");
        }

        if (!LUVK_EXECUTE(vkCreateSemaphore(DeviceMod->GetLogicalDevice(), &SemInfo, nullptr, &m_Frames[It].RenderFinished)))
        {
            throw std::runtime_error("Failed to create render semaphore");
        }

        if (!LUVK_EXECUTE(vkCreateFence(DeviceMod->GetLogicalDevice(), &FenceInfo, nullptr, &m_Frames[It].InFlight)))
        {
            throw std::runtime_error("Failed to create fence");
        }
    }

    IModule::OnInitialize(ServiceLocator);
}

void Synchronization::OnShutdown()
{
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    if (!DeviceMod)
    {
        return;
    }

    for (std::uint32_t It = 0; It < m_FrameCount; ++It)
    {
        if (m_Frames[It].ImageAvailable)
        {
            vkDestroySemaphore(DeviceMod->GetLogicalDevice(), m_Frames[It].ImageAvailable, nullptr);
        }

        if (m_Frames[It].RenderFinished)
        {
            vkDestroySemaphore(DeviceMod->GetLogicalDevice(), m_Frames[It].RenderFinished, nullptr);
        }

        if (m_Frames[It].InFlight)
        {
            vkDestroyFence(DeviceMod->GetLogicalDevice(), m_Frames[It].InFlight, nullptr);
        }
    }

    IModule::OnShutdown();
}

void Synchronization::AdvanceFrame()
{
    m_CurrentFrame = (m_CurrentFrame + 1) % m_FrameCount;
}

void Synchronization::WaitForCurrentFrame() const
{
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    vkWaitForFences(DeviceMod->GetLogicalDevice(), 1, &m_Frames[m_CurrentFrame].InFlight, VK_TRUE, UINT64_MAX);
    vkResetFences(DeviceMod->GetLogicalDevice(), 1, &m_Frames[m_CurrentFrame].InFlight);
}
