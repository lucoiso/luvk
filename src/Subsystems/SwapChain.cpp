// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Subsystems/SwapChain.hpp"
#include "luvk/Subsystems/Device.hpp"
#include "luvk/Subsystems/Renderer.hpp"
#include "luvk/Subsystems/Memory.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include <algorithm>
#include <iterator>
#include <array>

void luvk::SwapChain::CreateSwapChain(std::shared_ptr<IRenderModule> const& DeviceModule,
                                      std::shared_ptr<IRenderModule> const& MemoryModule,
                                      CreationArguments&& Arguments,
                                      void* const& pNext)
{
    m_DeviceModule = DeviceModule;
    auto const CastDeviceModule = std::dynamic_pointer_cast<luvk::Device>(DeviceModule);
    auto const CastMemoryModule = std::dynamic_pointer_cast<luvk::Memory>(MemoryModule);

    m_PreviousSwapChain = m_SwapChain;

    m_Arguments = Arguments;
    m_Extent = Arguments.Extent;

    VkSurfaceCapabilitiesKHR Caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(CastDeviceModule->GetPhysicalDevice(), CastDeviceModule->GetSurface(), &Caps);

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
                                                       .surface = CastDeviceModule->GetSurface(),
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

    VkDevice const& LogicalDevice = CastDeviceModule->GetLogicalDevice();

    if (!LUVK_EXECUTE(vkCreateSwapchainKHR(LogicalDevice, &SwapChainCreateInfo, nullptr, &m_SwapChain)))
    {
        throw std::runtime_error("Failed to (re) create the swap chain.");
    }

    bool const HasChangedNumImages = Arguments.ImageCount != std::size(m_Images);

    DestroySwapChainImages(LogicalDevice);
    DestroyDepthResources(LogicalDevice);
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
    CreateDepthResources(CastDeviceModule, CastMemoryModule);
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

    auto const DeviceModule = std::dynamic_pointer_cast<luvk::Device>(m_DeviceModule);
    VkDevice const& LogicalDevice = DeviceModule->GetLogicalDevice();

    DestroySwapChainImages(LogicalDevice);
    DestroyDepthResources(LogicalDevice);
    DestroyFramebuffers(LogicalDevice);
    DestroyRenderPass(LogicalDevice);

    if (m_SwapChain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(LogicalDevice, m_SwapChain, nullptr);
        m_SwapChain = VK_NULL_HANDLE;
    }
}

void luvk::SwapChain::Recreate(std::shared_ptr<IRenderModule> const& DeviceModule,
                               std::shared_ptr<IRenderModule> const& MemoryModule,
                               const VkExtent2D NewExtent,
                               void* const & pNext)
{
    m_DeviceModule = DeviceModule;
    m_Arguments.Extent = NewExtent;
    CreateSwapChain(DeviceModule, MemoryModule, CreationArguments(m_Arguments), pNext);
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
    const std::array Attachments{VkAttachmentDescription{.format = m_Arguments.Format,
                                                         .samples = VK_SAMPLE_COUNT_1_BIT,
                                                         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                         .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                                                         .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                         .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                         .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                         .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
                                 VkAttachmentDescription{.format = m_DepthFormat,
                                                         .samples = VK_SAMPLE_COUNT_1_BIT,
                                                         .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                                                         .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                         .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                                                         .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                                                         .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                                         .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};

    constexpr std::array AttachmentReferences{VkAttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
                                              VkAttachmentReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL}};

    VkSubpassDescription Subpass{.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
                                 .colorAttachmentCount = 1U,
                                 .pColorAttachments = &AttachmentReferences.at(0U),
                                 .pDepthStencilAttachment = &AttachmentReferences.at(1U)};

    VkSubpassDependency SubpassDependency{.srcSubpass = VK_SUBPASS_EXTERNAL,
                                          .dstSubpass = 0,
                                          .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                          .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                                                          VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                          .srcAccessMask = 0,
                                          .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT};

    const VkRenderPassCreateInfo Info{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
                                      .attachmentCount = static_cast<std::int32_t>(std::size(Attachments)),
                                      .pAttachments = std::data(Attachments),
                                      .subpassCount = 1,
                                      .pSubpasses = &Subpass,
                                      .dependencyCount = 1,
                                      .pDependencies = &SubpassDependency};

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
        std::array Views{m_ImageViews.at(FramebufferIndex),
                         m_DepthImages.at(FramebufferIndex)->GetView()};

        VkFramebufferCreateInfo Info{.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
                                     .renderPass = m_RenderPass,
                                     .attachmentCount = static_cast<std::int32_t>(std::size(Views)),
                                     .pAttachments = std::data(Views),
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

void luvk::SwapChain::CreateDepthResources(std::shared_ptr<Device> const& DeviceModule,
                                           std::shared_ptr<Memory> const& MemoryModule)
{
    m_DepthFormat = SelectDepthFormat(DeviceModule);
    bool const HasStencil = m_DepthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
            m_DepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            m_DepthFormat == VK_FORMAT_D16_UNORM_S8_UINT;
    m_DepthImages.reserve(std::size(m_Images));

    for (std::size_t Index = 0; Index < std::size(m_Images); ++Index)
    {
        const auto& DepthImage = m_DepthImages.emplace_back(std::make_shared<luvk::Image>());

        VkImageAspectFlags const Aspect = HasStencil
                                              ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                                              : static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT);

        DepthImage->CreateImage(DeviceModule,
                                MemoryModule,
                                {.Extent = {m_Extent.width, m_Extent.height, 1},
                                 .Format = m_DepthFormat,
                                 .Usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                 .Aspect = Aspect,
                                 .MemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY});
    }
}

void luvk::SwapChain::DestroyDepthResources(VkDevice const& LogicalDevice)
{
    for (auto& MemoryIt : m_DepthMemories)
    {
        if (MemoryIt != VK_NULL_HANDLE)
        {
            vkFreeMemory(LogicalDevice, MemoryIt, nullptr);
            MemoryIt = VK_NULL_HANDLE;
        }
    }

    m_DepthMemories.clear();
    m_DepthImages.clear();
}

VkFormat luvk::SwapChain::SelectDepthFormat(std::shared_ptr<Device> const& DeviceModule)
{
    for (constexpr std::array Candidates{VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM};
         const VkFormat Format : Candidates)
    {
        VkFormatProperties Props{};
        vkGetPhysicalDeviceFormatProperties(DeviceModule->GetPhysicalDevice(), Format, &Props);

        if (Props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return Format;
        }
    }

    return VK_FORMAT_D16_UNORM;
}
