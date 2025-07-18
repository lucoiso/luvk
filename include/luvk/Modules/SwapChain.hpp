// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Types/Vector.hpp"
#include "luvk/Module.hpp"
#include "luvk/Resources/Image.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

namespace luvk
{
    struct SwapChainCreationArguments
    {
        bool Clip{true};
        VkSwapchainCreateFlagsKHR Flags{};
        VkPresentModeKHR PresentMode{VK_PRESENT_MODE_FIFO_KHR};
        VkSurfaceTransformFlagBitsKHR TransformFlags{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};
        VkFormat Format{VK_FORMAT_R8G8B8A8_UNORM};
        VkColorSpaceKHR ColorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        VkImageUsageFlags UsageFlags{VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
        VkCompositeAlphaFlagBitsKHR CompositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
        std::uint32_t ImageCount{1U};
        VkExtent2D Extent{.width = 0U, .height = 0U};
        luvk::Vector<std::uint32_t> QueueIndices{};
    };

    enum class SwapChainEvents : std::uint8_t
    {
        OnCreated, OnRecreated, OnChangedNumberOfImages
    };

    class LUVKMODULE_API SwapChain : public IRenderModule
    {
        using CreationArguments = SwapChainCreationArguments;

        VkSwapchainKHR m_SwapChain{VK_NULL_HANDLE};
        VkSwapchainKHR m_PreviousSwapChain{VK_NULL_HANDLE};
        luvk::Vector<VkImage> m_Images{};
        luvk::Vector<VkImageView> m_ImageViews{};
        luvk::Vector<VkFramebuffer> m_Framebuffers{};
        luvk::Vector<std::shared_ptr<Image>> m_DepthImages{};
        VkFormat m_DepthFormat{VK_FORMAT_UNDEFINED};
        VkRenderPass m_RenderPass{VK_NULL_HANDLE};
        VkExtent2D m_Extent{0U, 0U};
        std::shared_ptr<IRenderModule> m_DeviceModule{};
        CreationArguments m_Arguments{};

    public:
        constexpr SwapChain() = default;

        ~SwapChain() override
        {
            SwapChain::ClearResources();
        }

        [[nodiscard]] luvk::UnorderedMap<std::string_view, luvk::Vector<std::string_view>> GetRequiredDeviceExtensions() const override
        {
            return ToExtensionMap("", {VK_KHR_SWAPCHAIN_EXTENSION_NAME});
        }

        [[nodiscard]] constexpr VkSwapchainKHR const& GetHandle() const
        {
            return m_SwapChain;
        }

        [[nodiscard]] constexpr luvk::Vector<VkImage> const& GetImages() const
        {
            return m_Images;
        }

        [[nodiscard]] constexpr luvk::Vector<VkImageView> const& GetImageViews() const
        {
            return m_ImageViews;
        }

        [[nodiscard]] constexpr VkFramebuffer const& GetFramebuffer(const std::size_t Index) const
        {
            return m_Framebuffers.at(Index);
        }

        [[nodiscard]] constexpr std::shared_ptr<Image> const& GetDepthImage(const std::size_t Index) const
        {
            return m_DepthImages.at(Index);
        }

        [[nodiscard]] constexpr luvk::Vector<std::shared_ptr<Image>> const& GetDepthImages() const
        {
            return m_DepthImages;
        }

        [[nodiscard]] constexpr VkFormat GetDepthFormat() const
        {
            return m_DepthFormat;
        }

        [[nodiscard]] constexpr VkRenderPass const& GetRenderPass() const
        {
            return m_RenderPass;
        }

        [[nodiscard]] constexpr VkExtent2D const& GetExtent() const
        {
            return m_Extent;
        }

        [[nodiscard]] constexpr CreationArguments const& GetCreationArguments() const
        {
            return m_Arguments;
        }

        void CreateSwapChain(std::shared_ptr<IRenderModule> const& DeviceModule,
                             std::shared_ptr<IRenderModule> const& MemoryModule,
                             CreationArguments&& Arguments,
                             void* const& pNext);

        void Recreate(std::shared_ptr<IRenderModule> const& DeviceModule, std::shared_ptr<IRenderModule> const& MemoryModule, VkExtent2D NewExtent, void* const& pNext);

    private:
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override;
        void ClearResources() override;
        void CreateSwapChainImages(VkDevice const& LogicalDevice);
        void DestroySwapChainImages(VkDevice const& LogicalDevice);
        void CreateRenderPass(VkDevice const& LogicalDevice);
        void DestroyRenderPass(VkDevice const& LogicalDevice);
        void CreateFramebuffers(VkDevice const& LogicalDevice);
        void DestroyFramebuffers(VkDevice const& LogicalDevice);
        void CreateDepthResources(std::shared_ptr<Device> const& DeviceModule, std::shared_ptr<Memory> const& MemoryModule);
        void DestroyDepthResources(VkDevice const& LogicalDevice);
        [[nodiscard]] static VkFormat SelectDepthFormat(std::shared_ptr<Device> const& DeviceModule);
    };
} // namespace luvk
