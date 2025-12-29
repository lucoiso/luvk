/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <optional>
#include <span>
#include <vector>
#include <volk.h>
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IModule.hpp"

namespace luvk
{
    class Image;
    struct FrameData;

    /**
     * Arguments for creating or recreating the Vulkan Swap Chain.
     */
    struct LUVK_API SwapChainCreationArguments
    {
        /** Creation flags for the swap chain. */
        VkSwapchainCreateFlagsKHR Flags{0};

        /** Presentation mode (e.g., VK_PRESENT_MODE_FIFO_KHR for VSync). */
        VkPresentModeKHR PresentMode{VK_PRESENT_MODE_FIFO_KHR};

        /** Pre-transform applied to the images (e.g., rotation). */
        VkSurfaceTransformFlagBitsKHR Transform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};

        /** Pixel format for the swap chain images. */
        VkFormat Format{VK_FORMAT_B8G8R8A8_UNORM};

        /** Color space for the swap chain images. */
        VkColorSpaceKHR ColorSpace{VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

        /** Usage flags for the swap chain images. */
        VkImageUsageFlags Usage{VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT};

        /** Composite alpha mode for blending with other windows. */
        VkCompositeAlphaFlagBitsKHR Alpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};

        /** Resolution of the swap chain images. */
        VkExtent2D Extent{0U, 0U};

        /** Whether to enable clipping outside the extent. */
        bool Clip{true};

        /** The Vulkan surface (window) to present to. */
        VkSurfaceKHR Surface{VK_NULL_HANDLE};
    };

    /**
     * Manages presentation images.
     * Pure Dynamic Rendering implementation (No RenderPass/Framebuffer).
     */
    class SwapChain : public IModule
                    , public IExtensionsModule
    {
        /** The Vulkan swap chain handle. */
        VkSwapchainKHR m_SwapChain{VK_NULL_HANDLE};

        /** List of images owned by the swap chain. */
        std::vector<VkImage> m_Images{};

        /** List of image views created for the swap chain images. */
        std::vector<VkImageView> m_Views{};

        /** The arguments used to create the current swap chain. */
        SwapChainCreationArguments m_Arguments{};

        /** Pointer to the central service locator. */
        IServiceLocator* m_ServiceLocator{nullptr};

    public:
        /** Default destructor. */
        ~SwapChain() override = default;

        /** Called upon module initialization. */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /** Called upon module shutdown (destroys the swap chain and image views). */
        void OnShutdown() override;

        /** Get the required device extensions (Swap Chain extension). */
        [[nodiscard]] ExtensionMap GetDeviceExtensions() const noexcept override
        {
            return {{"", {VK_KHR_SWAPCHAIN_EXTENSION_NAME}}};
        }

        /**
         * Creates a new Vulkan swap chain and its image views.
         * @param Arguments Configuration for the swap chain creation.
         */
        void CreateSwapChain(const SwapChainCreationArguments& Arguments);

        /**
         * Recreates the swap chain, typically after a window resize or surface change.
         * @param NewExtent The new resolution of the swap chain.
         */
        void Recreate(const VkExtent2D& NewExtent);

        /**
         * Acquires the index of the next available swap chain image.
         * @param Semaphore Semaphore to signal when the image is available.
         * @param Fence Optional fence to signal when the image is available.
         * @return Optional index of the acquired image, or nullopt if acquisition failed.
         */
        [[nodiscard]] std::optional<std::uint32_t> AcquireNextImage(VkSemaphore Semaphore, VkFence Fence = VK_NULL_HANDLE) const;

        /**
         * Presents the given image to the surface.
         * @param ImageIndex The index of the image to present.
         * @param WaitSemaphores Span of semaphores to wait on before presenting.
         */
        void Present(std::uint32_t ImageIndex, std::span<const VkSemaphore> WaitSemaphores) const;

        /** Get the underlying VkSwapchainKHR handle. */
        [[nodiscard]] constexpr VkSwapchainKHR GetHandle() const noexcept
        {
            return m_SwapChain;
        }

        /** Get the image format of the swap chain. */
        [[nodiscard]] constexpr VkFormat GetFormat() const noexcept
        {
            return m_Arguments.Format;
        }

        /** Get the extent (resolution) of the swap chain images. */
        [[nodiscard]] constexpr VkExtent2D GetExtent() const noexcept
        {
            return m_Arguments.Extent;
        }

        /** Get a span of the swap chain image handles. */
        [[nodiscard]] constexpr std::span<const VkImage> GetImages() const noexcept
        {
            return m_Images;
        }

        /** Get a span of the swap chain image view handles. */
        [[nodiscard]] constexpr std::span<const VkImageView> GetViews() const noexcept
        {
            return m_Views;
        }

        /** Get the total number of images in the swap chain. */
        [[nodiscard]] constexpr std::uint32_t GetImageCount() const noexcept
        {
            return static_cast<std::uint32_t>(std::size(m_Images));
        }
    };
}
