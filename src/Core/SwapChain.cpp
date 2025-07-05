// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/SwapChain.hpp"
#include "luvk/Core/Device.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include <algorithm>
#include <iterator>

void luvk::SwapChain::CreateSwapChain(std::shared_ptr<IRenderModule> const& DeviceModule, CreationArguments&& Arguments, void* const & pNext)
{
    m_DeviceModule = DeviceModule;
    auto const CastModule = dynamic_cast<luvk::Device*>(DeviceModule.get());

    m_PreviousSwapChain = m_SwapChain;

    m_Arguments = Arguments;
    m_Extent = Arguments.Extent;

    VkSurfaceCapabilitiesKHR Caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(CastModule->GetPhysicalDevice(), CastModule->GetSurface(), &Caps);

    if (m_Arguments.ImageCount == 0)
    {
        m_Arguments.ImageCount = Caps.minImageCount;
    }
    else
    {
        m_Arguments.ImageCount = std::clamp(m_Arguments.ImageCount, Caps.minImageCount, Caps.maxImageCount);
    }

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
                                                       .oldSwapchain = m_PreviousSwapChain};

    VkDevice const& LogicalDevice = CastModule->GetLogicalDevice();

    if (!LUVK_EXECUTE(vkCreateSwapchainKHR(LogicalDevice, &SwapChainCreateInfo, nullptr, &m_SwapChain)))
    {
        throw std::runtime_error("Failed to (re) create the swap chain.");
    }

    bool const HasChangedNumImages = Arguments.ImageCount != std::size(m_Images);

    DestroySwapChainImages(LogicalDevice);
    DestroyFramebuffers(LogicalDevice);
    DestroyRenderPass(LogicalDevice);
    DestroyFramebuffers(LogicalDevice);
    DestroyRenderPass(LogicalDevice);

    if (m_PreviousSwapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(LogicalDevice, m_PreviousSwapChain, nullptr);
        m_PreviousSwapChain = VK_NULL_HANDLE;
    }

    CreateSwapChainImages(LogicalDevice);
    CreateRenderPass(LogicalDevice);
    CreateFramebuffers(LogicalDevice);

    GetEventSystem().Execute(SwapChainEvents::OnCreated);

    if (HasChangedNumImages)
    {
        GetEventSystem().Execute(SwapChainEvents::OnChangedNumberOfImages);
    }
}

void luvk::SwapChain::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    // Do nothing
}

void luvk::SwapChain::ClearResources()
{
    if (!m_DeviceModule)
    {
        return;
    }
    auto const* DeviceModule = dynamic_cast<luvk::Device*>(m_DeviceModule.get());
    VkDevice const& LogicalDevice = DeviceModule->GetLogicalDevice();

    DestroySwapChainImages(LogicalDevice);
    DestroyFramebuffers(LogicalDevice);
    DestroyRenderPass(LogicalDevice);

    if (m_SwapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(LogicalDevice, m_SwapChain, nullptr);
        m_SwapChain = VK_NULL_HANDLE;
    }
}

void luvk::SwapChain::Recreate(std::shared_ptr<IRenderModule> const& DeviceModule, const VkExtent2D NewExtent, void* const & pNext)
{
    m_DeviceModule = DeviceModule;
    m_Arguments.Extent = NewExtent;
    CreateSwapChain(DeviceModule, CreationArguments(m_Arguments), pNext);
    GetEventSystem().Execute(SwapChainEvents::OnRecreated);
}

void luvk::SwapChain::CreateSwapChainImages(VkDevice const& LogicalDevice)
{
    std::uint32_t NumImages = 0U;
    if (!LUVK_EXECUTE(vkGetSwapchainImagesKHR(LogicalDevice, m_SwapChain, &NumImages, nullptr)))
    {
        throw std::runtime_error("Failed to get the number of swap chain images.");
    }

    m_Images.resize(NumImages);
    if (!LUVK_EXECUTE(vkGetSwapchainImagesKHR(LogicalDevice, m_SwapChain, &NumImages, std::data(m_Images))))
    {
        throw std::runtime_error("Failed to get the swap chain images.");
    }

    m_ImageViews.resize(std::size(m_Images));
    for (std::size_t Index = 0; Index < std::size(m_Images); ++Index)
    {
        VkImageViewCreateInfo const ViewInfo{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                             .image = m_Images.at(Index),
                                             .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                             .format = m_Arguments.Format,
                                             .components = {VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            VK_COMPONENT_SWIZZLE_IDENTITY,
                                                            VK_COMPONENT_SWIZZLE_IDENTITY},
                                             .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};

        if (!LUVK_EXECUTE(vkCreateImageView(LogicalDevice, &ViewInfo, nullptr, &m_ImageViews.at(Index))))
        {
            throw std::runtime_error("Failed to create swap chain image view.");
        }
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

    m_ImageViews.clear();
}

void luvk::SwapChain::CreateRenderPass(VkDevice const& LogicalDevice)
{
    VkAttachmentDescription colorAttachment{.format = m_Arguments.Format,
                                            .samples = VK_SAMPLE_COUNT_1_BIT,
                                            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                            .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR};

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount = 1, .pColorAttachments = &colorRef};

    VkSubpassDependency dependency{.srcSubpass = VK_SUBPASS_EXTERNAL,
                                   .dstSubpass = 0,
                                   .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   .srcAccessMask = 0,
                                   .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};

    const VkRenderPassCreateInfo Info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      .attachmentCount = 1,
                                      .pAttachments = &colorAttachment,
                                      .subpassCount = 1,
                                      .pSubpasses = &subpass,
                                      .dependencyCount = 1,
                                      .pDependencies = &dependency};

    LUVK_EXECUTE(vkCreateRenderPass(LogicalDevice, &Info, nullptr, &m_RenderPass));
}

void luvk::SwapChain::DestroyRenderPass(VkDevice const& LogicalDevice)
{
    if (m_RenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(LogicalDevice, m_RenderPass, nullptr);
        m_RenderPass = VK_NULL_HANDLE;
    }
}

void luvk::SwapChain::CreateFramebuffers(VkDevice const& LogicalDevice)
{
    m_Framebuffers.resize(std::size(m_ImageViews));
    for (std::size_t FramebufferIndex = 0; FramebufferIndex < std::size(m_ImageViews); ++FramebufferIndex)
    {
        VkImageView View = m_ImageViews.at(FramebufferIndex);
        VkFramebufferCreateInfo Info{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                     .renderPass = m_RenderPass,
                                     .attachmentCount = 1,
                                     .pAttachments = &View,
                                     .width = m_Extent.width,
                                     .height = m_Extent.height,
                                     .layers = 1};
        LUVK_EXECUTE(vkCreateFramebuffer(LogicalDevice, &Info, nullptr, &m_Framebuffers.at(FramebufferIndex)));
    }
}

void luvk::SwapChain::DestroyFramebuffers(VkDevice const& LogicalDevice)
{
    for (VkFramebuffer& FramebufferIt : m_Framebuffers)
    {
        if (FramebufferIt != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(LogicalDevice, FramebufferIt, nullptr);
            FramebufferIt = VK_NULL_HANDLE;
        }
    }
    m_Framebuffers.clear();
}
