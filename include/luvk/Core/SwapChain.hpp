// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <volk/volk.h>
#include <cstdint>
#include <vector>

namespace luvk
{
    /** Swap Chain Event Keys */
    enum class SwapChainEvents : std::uint8_t
    {
        OnChangedNumberOfImages
    };

    /** Render module responsible for the Swap Chain management */
    class LUVKMODULE_API SwapChain : public IRenderModule
    {
        VkSwapchainKHR m_SwapChain {VK_NULL_HANDLE};
        VkSwapchainKHR m_PreviousSwapChain {VK_NULL_HANDLE};
        std::vector<VkImage> m_Images{};
        std::vector<VkImageView> m_ImageViews{};

    public:
        constexpr SwapChain() = default;
        ~SwapChain() override = default;

        /** Arguments for the swap chain (re) creation */
        struct CreationArguments
        {
            bool Clip {true};
            VkSwapchainCreateFlagsKHR Flags {};
            VkPresentModeKHR PresentMode {VK_PRESENT_MODE_FIFO_KHR};
            VkSurfaceTransformFlagBitsKHR TransformFlags {VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};
            VkFormat Format {VK_FORMAT_R8G8B8A8_UNORM};
            VkColorSpaceKHR ColorSpace {VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
            VkImageUsageFlags UsageFlags {VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
            VkCompositeAlphaFlagBitsKHR CompositeAlpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
            std::uint32_t ImageCount {1U};
            VkExtent2D Extent {.width = 0U, .height = 0U};
            std::vector<std::uint32_t> QueueIndices {};
        };

        /** (Re) Create the Swap Chain */
        void CreateSwapChain(std::shared_ptr<IRenderModule> const& DeviceModule, CreationArguments&& Arguments, void* const& pNext);

    private:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources(IRenderModule* MainRenderer) override;

        /** Create the swap chain images */
        void CreateSwapChainImages(VkDevice const& LogicalDevice);

        /** Destroy the swap chain images */
        void DestroySwapChainImages(VkDevice const& LogicalDevice);
    };
} // namespace luvk