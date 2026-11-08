// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    class Context;
    class SwapchainProvider;
    struct SwapchainFunctions;

    /** Parameters used to create and recreate the swapchain. */
    struct SwapchainDesc
    {
        /** Surface to present to (required; owned by the caller, typically a PresentTarget). */
        VkSurfaceKHR Surface = nullptr;

        /** Used when the surface does not dictate its size (e.g. Wayland). */
        Extent2D Extent = {};

        /** Clamped to the surface limits. */
        std::uint32_t DesiredImageCount = 3U;

        /** First supported entry wins; falls back to the first format the surface reports. */
        std::vector<SurfaceFormat> PreferredFormats{{Format::B8G8R8A8Srgb, ColorSpace::SrgbNonlinear},
                                                    {Format::R8G8B8A8Srgb, ColorSpace::SrgbNonlinear},
                                                    {Format::B8G8R8A8Unorm, ColorSpace::SrgbNonlinear}};

        /** First supported entry wins; FIFO (always available) is the fallback. */
        std::vector<luvk::PresentMode> PreferredPresentModes = {luvk::PresentMode::Mailbox, luvk::PresentMode::Fifo};

        /** Image usage for the swapchain images (intersected with the surface's supported usages). */
        ImageUsage Usage = ImageUsage::ColorAttachment | ImageUsage::TransferDst;

        /** Preferred composite alpha mode; when unsupported, the first mode of the same alpha class is used. */
        luvk::CompositeAlpha PreferredCompositeAlpha = luvk::CompositeAlpha::Opaque;

        /** Optional owner of the swapchain handle and its entry points (e.g. a FidelityFX frame generation wrapper). */
        std::shared_ptr<SwapchainProvider> Provider;
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
     *
     * A SwapchainProvider (SwapchainDesc::Provider or SetProvider) takes over creation, destruction, image queries,
     * acquisition and presentation; present fences are then only used when the provider supports them.
     */
    class LUVK_API Swapchain
    {
        /** Owning context. */
        Context&                 m_Context;

        /** Creation parameters (extent is updated by Recreate). */
        SwapchainDesc            m_Desc;

        /** Surface the swapchain presents to (owned by the caller, never destroyed here). */
        VkSurfaceKHR m_Surface = nullptr;

        /** Vulkan swapchain handle. */
        VkSwapchainKHR m_Swapchain = nullptr;

        /** Selected surface format and color space. */
        SurfaceFormat m_Format = {};

        /** Selected present mode. */
        luvk::PresentMode m_PresentMode = luvk::PresentMode::Fifo;

        /** Selected composite alpha mode. */
        luvk::CompositeAlpha m_CompositeAlpha = luvk::CompositeAlpha::Opaque;

        /** Current image extent in pixels. */
        Extent2D m_Extent = {};

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

        /** Optional owner of the swapchain handle and its entry points. */
        std::shared_ptr<SwapchainProvider> m_Provider;

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
        bool Recreate(Extent2D RequestedExtent);

        /**
         * Installs (or removes, with nullptr) a swapchain provider and recreates the swapchain through it. Waits for the
         * graphics queue and pending presents first; must not be called while an acquired image is not presented yet.
         * Returns false when the surface has a zero extent (the swapchain is then created by the next Recreate).
         */
        bool SetProvider(std::shared_ptr<SwapchainProvider> Provider);

        /** Installed swapchain provider (null when the core Vulkan functions are used). */
        [[nodiscard]] const std::shared_ptr<SwapchainProvider>& Provider() const noexcept { return m_Provider; }

        /** Switches between FIFO (VSync) and the lowest-latency supported mode, recreating the swapchain. */
        bool SetVSync(bool Enabled);

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
        [[nodiscard]] bool               IsValid() const noexcept { return m_Swapchain != nullptr; }

        /** Vulkan swapchain handle. */
        [[nodiscard]] VkSwapchainKHR     Handle() const noexcept { return m_Swapchain; }

        /** Surface the swapchain presents to (owned by the caller). */
        [[nodiscard]] VkSurfaceKHR       Surface() const noexcept { return m_Surface; }

        /** Selected image format. */
        [[nodiscard]] luvk::Format       Format() const noexcept { return m_Format.Format; }

        /** Selected color space. */
        [[nodiscard]] luvk::ColorSpace   ColorSpace() const noexcept { return m_Format.ColorSpace; }

        /** Selected present mode. */
        [[nodiscard]] luvk::PresentMode  PresentMode() const noexcept { return m_PresentMode; }

        /** Selected composite alpha mode. */
        [[nodiscard]] luvk::CompositeAlpha CompositeAlpha() const noexcept { return m_CompositeAlpha; }

        /** Current image extent in pixels. */
        [[nodiscard]] Extent2D         Extent() const noexcept { return m_Extent; }

        /** Number of images in the swapchain. */
        [[nodiscard]] std::uint32_t      ImageCount() const noexcept { return static_cast<std::uint32_t>(std::size(m_Images)); }

        /** Image at Index (bounds-checked). */
        [[nodiscard]] VkImage            Image(const std::uint32_t Index) const { return m_Images.at(Index); }

        /** View of the image at Index (bounds-checked). */
        [[nodiscard]] VkImageView        View(const std::uint32_t Index) const { return m_Views.at(Index); }

        /** Render-finished semaphore of the image at Index (bounds-checked). */
        [[nodiscard]] VkSemaphore        RenderFinished(const std::uint32_t Index) const { return m_RenderFinished.at(Index); }

    private:
        /** Entry points to use: the provider's replacements, else the core functions. */
        [[nodiscard]] SwapchainFunctions ResolveFunctions() const;

        /** Destroys Handle through the provider or vkDestroySwapchainKHR. */
        void DestroyHandle(VkSwapchainKHR Handle) noexcept;

        /** Waits for every pending present fence (used before destruction and recreation). */
        void WaitForAllPresents() noexcept;

        /** Destroys the present fences, image views and semaphores, and clears the image lists. */
        void DestroyImageResources() noexcept;
    };
}
