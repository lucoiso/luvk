// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Render/Swapchain.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
#include <string>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"

luvk::Swapchain::Swapchain(Context& Ctx, SwapchainDesc Desc)
    : m_Context(Ctx),
      m_Desc(std::move(Desc)),
      m_Surface(m_Desc.Surface)
{
    if (m_Surface == VK_NULL_HANDLE)
    {
        throw std::logic_error("[luvk] A Swapchain requires SwapchainDesc::Surface.");
    }

    try
    {
        (void)Recreate(m_Desc.Extent);
    }
    catch (...)
    {
        DestroyImageResources();
        if (m_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_Context.Device(), m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
        throw;
    }
}

luvk::Swapchain::~Swapchain()
{

    try
    {
        m_Context.GraphicsQueue().WaitIdle();
    }
    catch (...)
    {

    }

    WaitForAllPresents();
    DestroyImageResources();
    if (m_Swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_Context.Device(), m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

bool luvk::Swapchain::Recreate(const VkExtent2D RequestedExtent)
{
    const VkPhysicalDevice PhysicalDevice = m_Context.PhysicalDevice();
    const VkSurfaceKHR     Surface        = m_Surface;
    const VkDevice         Device         = m_Context.Device();

    m_Desc.Extent = RequestedExtent;

    VkSurfaceCapabilitiesKHR Capabilities = {};
    Check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(PhysicalDevice, Surface, &Capabilities), "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");

    VkExtent2D Extent = Capabilities.currentExtent;
    if (Extent.width == UINT32_MAX)
    {
        Extent.width  = std::clamp(RequestedExtent.width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
        Extent.height = std::clamp(RequestedExtent.height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);
    }

    // A minimised window keeps reporting a zero extent: bail out before stalling the queue every frame.
    if (Extent.width == 0U || Extent.height == 0U)
    {
        return false;
    }

    m_Context.GraphicsQueue().WaitIdle();
    WaitForAllPresents();

    const auto Formats = Enumerate<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, PhysicalDevice, Surface);
    const auto Modes   = Enumerate<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, PhysicalDevice, Surface);

    if (std::empty(Formats))
    {
        throw std::runtime_error("[luvk] The surface reports no formats.");
    }

    m_Format = Formats.front();
    for (const VkSurfaceFormatKHR& Wanted : m_Desc.PreferredFormats)
    {
        if (std::ranges::any_of(Formats,
                                [&](const VkSurfaceFormatKHR& It)
                                {
                                    return It.format == Wanted.format && It.colorSpace == Wanted.colorSpace;
                                }))
        {
            m_Format = Wanted;
            break;
        }
    }

    m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const VkPresentModeKHR Wanted : m_Desc.PreferredPresentModes)
    {
        if (std::ranges::find(Modes, Wanted) != std::end(Modes))
        {
            m_PresentMode = Wanted;
            break;
        }
    }

    std::uint32_t ImageCount = std::max(m_Desc.DesiredImageCount, Capabilities.minImageCount);
    if (Capabilities.maxImageCount != 0U)
    {
        ImageCount = std::min(ImageCount, Capabilities.maxImageCount);
    }

    const VkImageUsageFlags Usage = m_Desc.Usage & Capabilities.supportedUsageFlags;
    if ((Usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0U)
    {
        throw std::runtime_error("[luvk] The surface does not support color attachment usage.");
    }

    VkCompositeAlphaFlagBitsKHR CompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if ((Capabilities.supportedCompositeAlpha & m_Desc.PreferredCompositeAlpha) != 0U)
    {
        CompositeAlpha = m_Desc.PreferredCompositeAlpha;
    }
    else
    {
        // A caller asking for transparency should keep an alpha mode if the preferred one is missing; a
        // caller asking for opaque should stay opaque. Within each group, prefer the standard modes.
        const bool PreferAlpha = m_Desc.PreferredCompositeAlpha != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        const std::array<VkCompositeAlphaFlagBitsKHR, 4> Candidates = PreferAlpha
                                                                          ? std::array{VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
                                                                                       VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                                                                       VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
                                                                                       VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR}
                                                                          : std::array{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                                                                                       VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
                                                                                       VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
                                                                                       VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR};

        for (const VkCompositeAlphaFlagBitsKHR Candidate : Candidates)
        {
            if ((Capabilities.supportedCompositeAlpha & Candidate) != 0U)
            {
                CompositeAlpha = Candidate;
                break;
            }
        }
    }
    m_CompositeAlpha = CompositeAlpha;

    const VkSwapchainKHR Old = m_Swapchain;

    const VkSwapchainCreateInfoKHR CreateInfo{.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                              .surface          = Surface,
                                              .minImageCount    = ImageCount,
                                              .imageFormat      = m_Format.format,
                                              .imageColorSpace  = m_Format.colorSpace,
                                              .imageExtent      = Extent,
                                              .imageArrayLayers = 1U,
                                              .imageUsage       = Usage,
                                              .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                              .preTransform     = Capabilities.currentTransform,
                                              .compositeAlpha   = CompositeAlpha,
                                              .presentMode      = m_PresentMode,
                                              .clipped          = VK_TRUE,
                                              .oldSwapchain     = Old};

    VkSwapchainKHR Created = VK_NULL_HANDLE;
    Check(vkCreateSwapchainKHR(Device, &CreateInfo, nullptr, &Created), "vkCreateSwapchainKHR");

    DestroyImageResources();
    if (Old != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(Device, Old, nullptr);
    }

    m_Swapchain = Created;
    m_Extent    = Extent;
    m_Images    = Enumerate<VkImage>(vkGetSwapchainImagesKHR, Device, m_Swapchain);

    m_Views.resize(std::size(m_Images), VK_NULL_HANDLE);
    m_RenderFinished.resize(std::size(m_Images), VK_NULL_HANDLE);

    if (m_Context.SupportsPresentFences())
    {
        constexpr VkFenceCreateInfo FenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
        m_PresentFences.resize(std::size(m_Images), VK_NULL_HANDLE);
        m_PresentPending.assign(std::size(m_Images), 0);
        for (VkFence& Fence : m_PresentFences)
        {
            Check(vkCreateFence(Device, &FenceInfo, nullptr, &Fence), "vkCreateFence (present)");
        }
    }

    constexpr VkSemaphoreCreateInfo SemaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    for (std::size_t Index = 0U; Index < std::size(m_Images); ++Index)
    {
        const VkImageViewCreateInfo ViewInfo{.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                             .image            = m_Images[Index],
                                             .viewType         = VK_IMAGE_VIEW_TYPE_2D,
                                             .format           = m_Format.format,
                                             .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0U, 1U, 0U, 1U}};
        Check(vkCreateImageView(Device, &ViewInfo, nullptr, &m_Views[Index]), "vkCreateImageView (swapchain)");
        Check(vkCreateSemaphore(Device, &SemaphoreInfo, nullptr, &m_RenderFinished[Index]), "vkCreateSemaphore (render finished)");

        const std::string Name = "luvk swapchain image " + std::to_string(Index);
        m_Context.SetDebugName(m_Images[Index], VK_OBJECT_TYPE_IMAGE, Name);
        m_Context.SetDebugName(m_Views[Index], VK_OBJECT_TYPE_IMAGE_VIEW, Name);
    }

    return true;
}

luvk::SwapchainStatus luvk::Swapchain::Acquire(const VkSemaphore Signal, std::uint32_t& OutImageIndex) const
{
    if (m_Swapchain == VK_NULL_HANDLE)
    {
        return SwapchainStatus::OutOfDate;
    }

    const VkResult Result = vkAcquireNextImageKHR(m_Context.Device(), m_Swapchain, UINT64_MAX, Signal, VK_NULL_HANDLE, &OutImageIndex);
    switch (Result)
    {
    case VK_SUCCESS: return SwapchainStatus::Ok;
    case VK_SUBOPTIMAL_KHR: return SwapchainStatus::Suboptimal;
    case VK_ERROR_OUT_OF_DATE_KHR: return SwapchainStatus::OutOfDate;
    default:
        Check(Result, "vkAcquireNextImageKHR");
        return SwapchainStatus::OutOfDate;
    }
}

luvk::SwapchainStatus luvk::Swapchain::Present(const std::uint32_t ImageIndex)
{
    const VkSemaphore Wait = m_RenderFinished.at(ImageIndex);

    VkSwapchainPresentFenceInfoKHR FenceInfo{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_KHR, .swapchainCount = 1U};
    const bool                     UseFence = !std::empty(m_PresentFences);
    if (UseFence)
    {
        // The caller already waited (WaitForPresent before its submission), so the fence is idle here.
        Check(vkResetFences(m_Context.Device(), 1U, &m_PresentFences[ImageIndex]), "vkResetFences (present)");
        FenceInfo.pFences = &m_PresentFences[ImageIndex];
    }

    const VkPresentInfoKHR Info{.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                .pNext              = UseFence ? &FenceInfo : nullptr,
                                .waitSemaphoreCount = 1U,
                                .pWaitSemaphores    = &Wait,
                                .swapchainCount     = 1U,
                                .pSwapchains        = &m_Swapchain,
                                .pImageIndices      = &ImageIndex};

    const VkResult Result = m_Context.GraphicsQueue().Present(Info);

    if (UseFence && (Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR || Result == VK_ERROR_OUT_OF_DATE_KHR))
    {
        m_PresentPending[ImageIndex] = 1;
    }

    switch (Result)
    {
    case VK_SUCCESS: return SwapchainStatus::Ok;
    case VK_SUBOPTIMAL_KHR: return SwapchainStatus::Suboptimal;
    case VK_ERROR_OUT_OF_DATE_KHR: return SwapchainStatus::OutOfDate;
    default:
        Check(Result, "vkQueuePresentKHR");
        return SwapchainStatus::OutOfDate;
    }
}

void luvk::Swapchain::WaitForPresent(const std::uint32_t ImageIndex)
{
    if (ImageIndex < std::size(m_PresentFences) && m_PresentPending[ImageIndex] != 0)
    {
        Check(vkWaitForFences(m_Context.Device(), 1U, &m_PresentFences[ImageIndex], VK_TRUE, UINT64_MAX), "vkWaitForFences (present)");
        m_PresentPending[ImageIndex] = 0;
    }
}

void luvk::Swapchain::WaitForAllPresents() noexcept
{
    for (std::uint32_t Index = 0U; Index < std::size(m_PresentFences); ++Index)
    {
        try
        {
            WaitForPresent(Index);
        }
        catch (...)
        {
            m_PresentPending[Index] = 0;
        }
    }
}

void luvk::Swapchain::DestroyImageResources() noexcept
{
    const VkDevice Device = m_Context.Device();

    for (const VkFence Fence : m_PresentFences)
    {
        if (Fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(Device, Fence, nullptr);
        }
    }
    m_PresentFences.clear();
    m_PresentPending.clear();

    for (VkImageView& View : m_Views)
    {
        if (View != VK_NULL_HANDLE)
        {
            vkDestroyImageView(Device, View, nullptr);
            View = VK_NULL_HANDLE;
        }
    }

    for (VkSemaphore& Semaphore : m_RenderFinished)
    {
        if (Semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(Device, Semaphore, nullptr);
            Semaphore = VK_NULL_HANDLE;
        }
    }

    m_Views.clear();
    m_RenderFinished.clear();
    m_Images.clear();
}
