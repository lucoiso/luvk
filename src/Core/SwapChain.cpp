// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/SwapChain.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Device.hpp"

void luvk::SwapChain::CreateSwapChain(std::shared_ptr<IRenderModule> const& DeviceModule, CreationArguments &&Arguments, void* const& pNext)
{
    auto const CastModule = static_cast<luvk::Device*>(DeviceModule.get());

    VkSwapchainCreateInfoKHR const SwapChainCreateInfo{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                                       .pNext = pNext,
                                                       .flags = Arguments.Flags,
                                                       .surface = CastModule->GetSurface(),
                                                       .minImageCount = Arguments.ImageCount,
                                                       .imageFormat = Arguments.Format,
                                                       .imageColorSpace = Arguments.ColorSpace,
                                                       .imageExtent = Arguments.Extent,
                                                       .imageArrayLayers = 1U,
                                                       .imageUsage = Arguments.UsageFlags,
                                                       .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                                       .queueFamilyIndexCount = static_cast<std::uint32_t>(std::size(Arguments.QueueIndices)),
                                                       .pQueueFamilyIndices = std::data(Arguments.QueueIndices),
                                                       .preTransform = Arguments.TransformFlags,
                                                       .compositeAlpha = Arguments.CompositeAlpha,
                                                       .presentMode = Arguments.PresentMode,
                                                       .clipped = Arguments.Clip,
                                                       .oldSwapchain = m_SwapChain};

    VkDevice const& LogicalDevice = CastModule->GetLogicalDevice();

    if (vkCreateSwapchainKHR(LogicalDevice, &SwapChainCreateInfo, nullptr, &m_SwapChain) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to (re) create the swap chain.");
    }

    bool const HasChangedNumImages = Arguments.ImageCount != std::size(m_Images);

    DestroySwapChainImages(LogicalDevice);
    CreateSwapChainImages(LogicalDevice);

    if (HasChangedNumImages)
    {
        GetEventSystem().Execute(SwapChainEvents::OnChangedNumberOfImages);
    }
}

void luvk::SwapChain::InitializeDependencies(std::shared_ptr<IRenderModule> const &MainRenderer)
{
    // Do nothing: Needs physical & logical devices to be ready
}

void luvk::SwapChain::ClearResources(IRenderModule *MainRenderer)
{
    auto const* DeviceModule = static_cast<luvk::Renderer*>(MainRenderer)->FindModule<luvk::Device>();
    VkDevice const& LogicalDevice = DeviceModule->GetLogicalDevice();

    DestroySwapChainImages(LogicalDevice);

    if (m_SwapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(LogicalDevice, m_SwapChain, nullptr);
        m_SwapChain = VK_NULL_HANDLE;
    }
}

void luvk::SwapChain::CreateSwapChainImages(VkDevice const& LogicalDevice)
{
    std::uint32_t NumImages = 0U;
    if (vkGetSwapchainImagesKHR(LogicalDevice, m_SwapChain, &NumImages, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to get the number of swap chain images.");
    }

    m_Images.resize(NumImages);
    if (vkGetSwapchainImagesKHR(LogicalDevice, m_SwapChain, &NumImages, std::data(m_Images)) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to get the swap chain images.");
    }
}

void luvk::SwapChain::DestroySwapChainImages(VkDevice const& LogicalDevice)
{
    for (VkImageView& ImageViewIt : m_ImageViews)
    {
        if (ImageViewIt != VK_NULL_HANDLE)
        {
            vkDestroyImageView(LogicalDevice, ImageViewIt, nullptr);
            ImageViewIt = VK_NULL_HANDLE;
        }
    }
}
