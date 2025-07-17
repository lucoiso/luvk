// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** SwapChain.hpp definitions */

#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Resources/Image.hpp"
#include <vector>

namespace luvk
{
    /** Swap Chain Event Keys */
    enum class SwapChainEvents : std::uint8_t
    {
        OnCreated, OnRecreated, OnChangedNumberOfImages
    };

    /** Render module responsible for the Swap Chain management */
    class LUVKMODULE_API SwapChain : public IRenderModule
    {
        /** Current swap chain handle */
        VkSwapchainKHR m_SwapChain{VK_NULL_HANDLE};

        /** Previous swap chain when recreating */
        VkSwapchainKHR m_PreviousSwapChain{VK_NULL_HANDLE};

        /** Images acquired from the swap chain */
        std::vector<VkImage> m_Images{};

        /** Image views for each swap chain image */
        std::vector<VkImageView> m_ImageViews{};

        /** Framebuffers for each swap chain image */
        std::vector<VkFramebuffer> m_Framebuffers{};

        /** Depth images for each framebuffer */
        std::vector<std::shared_ptr<luvk::Image>> m_DepthImages{};


        /** Format used for depth images */
        VkFormat m_DepthFormat{VK_FORMAT_UNDEFINED};

        /** Render pass used for presentation */
        VkRenderPass m_RenderPass{VK_NULL_HANDLE};

        /** Current swap chain extent */
        VkExtent2D m_Extent{0U, 0U};

        /** Device module used for creation */
        std::shared_ptr<IRenderModule> m_DeviceModule{};

        /** Arguments for the swap chain (re)creation */
        struct CreationArguments
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
            std::vector<std::uint32_t> QueueIndices{};
        };

        /** Stored creation arguments */
        CreationArguments m_Arguments{};


    public:
        constexpr SwapChain() = default;

        //~ Begin of IRenderModule interface
        ~SwapChain() override
        {
            SwapChain::ClearResources();
        }

        [[nodiscard]] std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredDeviceExtensions() const override
        {
            return ToExtensionMap("", {VK_KHR_SWAPCHAIN_EXTENSION_NAME});
        }

        /** Retrieve the swap chain handle */
        [[nodiscard]] constexpr VkSwapchainKHR const& GetHandle() const
        {
            return m_SwapChain;
        }

        /** Get the swap chain images */
        [[nodiscard]] constexpr std::vector<VkImage> const& GetImages() const
        {
            return m_Images;
        }

        /** Get the swap chain image views */
        [[nodiscard]] constexpr std::vector<VkImageView> const& GetImageViews() const
        {
            return m_ImageViews;
        }

        /** Get the framebuffer for the specified image */
        [[nodiscard]] constexpr VkFramebuffer const& GetFramebuffer(const std::size_t Index) const
        {
            return m_Framebuffers.at(Index);
        }

        /** Get the depth image for the specified index */
        [[nodiscard]] constexpr std::shared_ptr<luvk::Image> const& GetDepthImage(const std::size_t Index) const
        {
            return m_DepthImages.at(Index);
        }

        /** Retrieve all depth images */
        [[nodiscard]] constexpr std::vector<std::shared_ptr<luvk::Image>> const& GetDepthImages() const
        {
            return m_DepthImages;
        }

        /** Retrieve the format used for depth images */
        [[nodiscard]] constexpr VkFormat GetDepthFormat() const
        {
            return m_DepthFormat;
        }

        /** Retrieve the render pass used for drawing */
        [[nodiscard]] constexpr VkRenderPass const& GetRenderPass() const
        {
            return m_RenderPass;
        }

        /** Get the swap chain extent */
        [[nodiscard]] constexpr VkExtent2D const& GetExtent() const
        {
            return m_Extent;
        }

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const& DeviceModule) const noexcept override
        {
            return nullptr;
        }

        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

        /** (Re) Create the Swap Chain */
        void CreateSwapChain(std::shared_ptr<IRenderModule> const& DeviceModule,
                             std::shared_ptr<IRenderModule> const& MemoryModule,
                             CreationArguments&& Arguments,
                             void* const& pNext);

        /** Recreate using stored arguments and a new extent */
        void Recreate(std::shared_ptr<IRenderModule> const& DeviceModule, std::shared_ptr<IRenderModule> const& MemoryModule, VkExtent2D NewExtent, void* const& pNext);

    private:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override;

        /** Clear the resources of this module */
        void ClearResources() override;
        //~ End of IRenderModule interface

        /** Create the swap chain images */
        void CreateSwapChainImages(VkDevice const& LogicalDevice);

        /** Destroy the swap chain images */
        void DestroySwapChainImages(VkDevice const& LogicalDevice);

        /** Create the render pass object */
        void CreateRenderPass(VkDevice const& LogicalDevice);

        /** Destroy the render pass object */
        void DestroyRenderPass(VkDevice const& LogicalDevice);

        /** Create framebuffers for each image view */
        void CreateFramebuffers(VkDevice const& LogicalDevice);

        /** Destroy existing framebuffers */
        void DestroyFramebuffers(VkDevice const& LogicalDevice);

        /** Create depth images and views */
        void CreateDepthResources(std::shared_ptr<Device> const& DeviceModule, std::shared_ptr<Memory> const& MemoryModule);

        /** Destroy depth images and views */
        void DestroyDepthResources(VkDevice const& LogicalDevice);

        /** Select a supported format for depth resources */
        [[nodiscard]] static VkFormat SelectDepthFormat(std::shared_ptr<Device> const& DeviceModule);
    };
} // namespace luvk
