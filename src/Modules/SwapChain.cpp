/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/SwapChain.hpp"
#include <limits>
#include <stdexcept>
#include "luvk/Interfaces/IServiceLocator.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

void SwapChain::OnInitialize(IServiceLocator* ServiceLocator)
{
    m_ServiceLocator = ServiceLocator;
    IModule::OnInitialize(ServiceLocator);
}

void SwapChain::OnShutdown()
{
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    if (!DeviceMod)
    {
        return;
    }

    for (const auto View : m_Views)
    {
        vkDestroyImageView(DeviceMod->GetLogicalDevice(), View, nullptr);
    }

    m_Views.clear();

    if (m_SwapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(DeviceMod->GetLogicalDevice(), m_SwapChain, nullptr);
        m_SwapChain = VK_NULL_HANDLE;
    }

    IModule::OnShutdown();
}

void SwapChain::CreateSwapChain(const SwapChainCreationArguments& Arguments)
{
    m_Arguments           = Arguments;
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();

    VkSurfaceCapabilitiesKHR Caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(DeviceMod->GetPhysicalDevice(), Arguments.Surface, &Caps);

    std::uint32_t MinImageCount = Caps.minImageCount + 1;
    if (Caps.maxImageCount > 0 && MinImageCount > Caps.maxImageCount)
    {
        MinImageCount = Caps.maxImageCount;
    }

    VkExtent2D Extent = Arguments.Extent;
    if (Caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
    {
        Extent = Caps.currentExtent;
    }
    m_Arguments.Extent = Extent;

    const VkSwapchainCreateInfoKHR Info{.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                        .surface          = Arguments.Surface,
                                        .minImageCount    = MinImageCount,
                                        .imageFormat      = Arguments.Format,
                                        .imageColorSpace  = Arguments.ColorSpace,
                                        .imageExtent      = Extent,
                                        .imageArrayLayers = 1,
                                        .imageUsage       = Arguments.Usage,
                                        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                        .preTransform     = Arguments.Transform,
                                        .compositeAlpha   = Arguments.Alpha,
                                        .presentMode      = Arguments.PresentMode,
                                        .clipped          = Arguments.Clip ? VK_TRUE : VK_FALSE,
                                        .oldSwapchain     = m_SwapChain};

    VkSwapchainKHR NewSwapChain;
    if (!LUVK_EXECUTE(vkCreateSwapchainKHR(DeviceMod->GetLogicalDevice(), &Info, nullptr, &NewSwapChain)))
    {
        throw std::runtime_error("Failed to create SwapChain");
    }

    if (m_SwapChain != VK_NULL_HANDLE)
    {
        for (const auto View : m_Views)
        {
            vkDestroyImageView(DeviceMod->GetLogicalDevice(), View, nullptr);
        }

        vkDestroySwapchainKHR(DeviceMod->GetLogicalDevice(), m_SwapChain, nullptr);
    }

    m_SwapChain = NewSwapChain;

    std::uint32_t Count = 0;
    vkGetSwapchainImagesKHR(DeviceMod->GetLogicalDevice(), m_SwapChain, &Count, nullptr);
    m_Images.resize(Count);
    vkGetSwapchainImagesKHR(DeviceMod->GetLogicalDevice(), m_SwapChain, &Count, std::data(m_Images));

    m_Views.resize(Count);
    for (std::uint32_t It = 0; It < Count; ++It)
    {
        const VkImageViewCreateInfo ViewInfo{.sType      = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                             .image      = m_Images[It],
                                             .viewType   = VK_IMAGE_VIEW_TYPE_2D,
                                             .format     = Arguments.Format,
                                             .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            VK_COMPONENT_SWIZZLE_IDENTITY},
                                             .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        vkCreateImageView(DeviceMod->GetLogicalDevice(), &ViewInfo, nullptr, &m_Views[It]);
    }
}

void SwapChain::Recreate(const VkExtent2D& NewExtent)
{
    m_Arguments.Extent = NewExtent;
    CreateSwapChain(m_Arguments);
}

std::optional<std::uint32_t> SwapChain::AcquireNextImage(const VkSemaphore Semaphore, const VkFence Fence) const
{
    const auto*    DeviceMod = m_ServiceLocator->GetModule<Device>();
    std::uint32_t  Index;
    const VkResult Result = vkAcquireNextImageKHR(DeviceMod->GetLogicalDevice(), m_SwapChain, UINT64_MAX, Semaphore, Fence, &Index);

    if (Result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return std::nullopt;
    }
    if (Result != VK_SUCCESS && Result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire image");
    }

    return Index;
}

void SwapChain::Present(std::uint32_t ImageIndex, std::span<const VkSemaphore> WaitSemaphores) const
{
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();

    const VkPresentInfoKHR Info{.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .waitSemaphoreCount = static_cast<std::uint32_t>(std::size(WaitSemaphores)),
                                .pWaitSemaphores    = std::data(WaitSemaphores),
                                .swapchainCount     = 1,
                                .pSwapchains        = &m_SwapChain,
                                .pImageIndices      = &ImageIndex};

    const VkResult Result = vkQueuePresentKHR(DeviceMod->GetQueue(VK_QUEUE_GRAPHICS_BIT), &Info);

    if (Result != VK_SUCCESS && Result != VK_SUBOPTIMAL_KHR && Result != VK_ERROR_OUT_OF_DATE_KHR)
    {
        throw std::runtime_error("Failed to present");
    }
}
