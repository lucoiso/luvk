// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    class Context;

    /** Parameters used to create and recreate the swapchain. */
    struct SwapchainDesc
    {
        /** Surface to present to (required; owned by the caller, typically a PresentTarget). */
        VkSurfaceKHR Surface = VK_NULL_HANDLE;

        /** Used when the surface does not dictate its size (e.g. Wayland). */
        VkExtent2D Extent = {};

        /** Clamped to the surface limits. */
        std::uint32_t DesiredImageCount = 3U;

        /** First supported entry wins; falls back to the first format the surface reports. */
        std::vector<VkSurfaceFormatKHR> PreferredFormats{{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                                                         {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                                                         {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};

        /** First supported entry wins; FIFO (always available) is the fallback. */
        std::vector<VkPresentModeKHR> PreferredPresentModes = {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR};

        /** Image usage for the swapchain images (intersected with the surface's supported usages). */
        VkImageUsageFlags Usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        /** Preferred composite alpha mode; when unsupported, the first mode of the same alpha class is used. */
        VkCompositeAlphaFlagBitsKHR PreferredCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    };

    /** Outcome of an Acquire or Present call. */
    enum class SwapchainStatus : std::uint8_t
    {
        Ok,
        Suboptimal,
        OutOfDate,
    };

    /**
     * Window swapchain with validated format / present mode / extent / image count, one render-finished
     * semaphore per image and safe recreation (handles minimised windows: extent 0 -> not renderable).
     *
     * With VK_KHR/EXT_swapchain_maintenance1 every present signals a fence, so a render-finished semaphore
     * is only re-signalled (and the old swapchain only destroyed) once presentation is done with it.
     * Without it, recreation falls back to waiting for the graphics queue to be idle.
     */
    class LUVK_API Swapchain
    {
        /** Owning context. */
        Context&                 m_Context;

        /** Creation parameters (extent is updated by Recreate). */
        SwapchainDesc            m_Desc;

        /** Surface the swapchain presents to (owned by the caller, never destroyed here). */
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        /** Vulkan swapchain handle. */
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;

        /** Selected surface format and color space. */
        VkSurfaceFormatKHR m_Format = {};

        /** Selected present mode. */
        VkPresentModeKHR m_PresentMode = VK_PRESENT_MODE_FIFO_KHR;

        /** Selected composite alpha mode. */
        VkCompositeAlphaFlagBitsKHR m_CompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        /** Current image extent in pixels. */
        VkExtent2D m_Extent = {};

        /** Swapchain images. */
        std::vector<VkImage> m_Images;

        /** One 2D color view per image. */
        std::vector<VkImageView> m_Views;

        /** One render-finished semaphore per image (signalled by the submit that renders it). */
        std::vector<VkSemaphore> m_RenderFinished;

        /** One present fence per image (empty without swapchain_maintenance1). */
        std::vector<VkFence> m_PresentFences;

        /** Per image: 1 while its present fence is still in flight. */
        std::vector<char> m_PresentPending;

    public:
        /** Constructor; creates the swapchain (throws when Desc.Surface is not set). */
        Swapchain(Context& Ctx, SwapchainDesc Desc);

        /** Destructor. */
        ~Swapchain();

        /** Non-copyable. */
        Swapchain(const Swapchain&)            = delete;

        /** Non-copyable. */
        Swapchain& operator=(const Swapchain&) = delete;

        /** Recreates the swapchain. Returns false when the surface has a zero extent (minimised window). */
        bool Recreate(VkExtent2D RequestedExtent);

        /** Acquires the next image, signalling Signal; reports the status and the image index in OutImageIndex. */
        [[nodiscard]] SwapchainStatus Acquire(VkSemaphore Signal, std::uint32_t& OutImageIndex) const;

        /**
         * Presents ImageIndex, waiting on its render-finished semaphore; returns the present status.
         * WaitForPresent(ImageIndex) must have been called before the submission that signalled the semaphore.
         */
        [[nodiscard]] SwapchainStatus Present(std::uint32_t ImageIndex);

        /**
         * Blocks until the previous presentation of ImageIndex released its render-finished semaphore.
         * Call before submitting work that signals RenderFinished(ImageIndex). No-op without present fences.
         */
        void WaitForPresent(std::uint32_t ImageIndex);

        /** True when a swapchain handle exists. */
        [[nodiscard]] bool               IsValid() const noexcept { return m_Swapchain != VK_NULL_HANDLE; }

        /** Vulkan swapchain handle. */
        [[nodiscard]] VkSwapchainKHR     Handle() const noexcept { return m_Swapchain; }

        /** Surface the swapchain presents to (owned by the caller). */
        [[nodiscard]] VkSurfaceKHR       Surface() const noexcept { return m_Surface; }

        /** Selected image format. */
        [[nodiscard]] VkFormat           Format() const noexcept { return m_Format.format; }

        /** Selected color space. */
        [[nodiscard]] VkColorSpaceKHR    ColorSpace() const noexcept { return m_Format.colorSpace; }

        /** Selected present mode. */
        [[nodiscard]] VkPresentModeKHR   PresentMode() const noexcept { return m_PresentMode; }

        /** Selected composite alpha mode. */
        [[nodiscard]] VkCompositeAlphaFlagBitsKHR CompositeAlpha() const noexcept { return m_CompositeAlpha; }

        /** Current image extent in pixels. */
        [[nodiscard]] VkExtent2D         Extent() const noexcept { return m_Extent; }

        /** Number of images in the swapchain. */
        [[nodiscard]] std::uint32_t      ImageCount() const noexcept { return static_cast<std::uint32_t>(std::size(m_Images)); }

        /** Image at Index (bounds-checked). */
        [[nodiscard]] VkImage            Image(const std::uint32_t Index) const { return m_Images.at(Index); }

        /** View of the image at Index (bounds-checked). */
        [[nodiscard]] VkImageView        View(const std::uint32_t Index) const { return m_Views.at(Index); }

        /** Render-finished semaphore of the image at Index (bounds-checked). */
        [[nodiscard]] VkSemaphore        RenderFinished(const std::uint32_t Index) const { return m_RenderFinished.at(Index); }

    private:
        /** Waits for every pending present fence (used before destruction and recreation). */
        void WaitForAllPresents() noexcept;

        /** Destroys the present fences, image views and semaphores, and clears the image lists. */
        void DestroyImageResources() noexcept;
    };
}
