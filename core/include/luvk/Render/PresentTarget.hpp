// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "luvk/Core/Common.hpp"
#include "luvk/Render/RenderGraph.hpp"
#include "luvk/Render/Swapchain.hpp"
#include "luvk/Resources/ResourceState.hpp"

namespace luvk
{
    class Context;

    /** Creation parameters of a PresentTarget. */
    struct PresentTargetDesc
    {
        /** Surface to present to (required). */
        VkSurfaceKHR Surface = VK_NULL_HANDLE;

        /** Destroys Surface together with the target (true for surfaces created for this target). */
        bool OwnsSurface = true;

        /** Swapchain parameters; SwapchainDesc::Surface is ignored (Surface above is used). */
        SwapchainDesc Swapchain = {};

        /** Frames in flight of the Renderer presenting this target (one acquire semaphore per frame slot). */
        std::uint32_t FramesInFlight = 2U;

        /** Debug name of the imported swapchain image. */
        std::string_view Name = "backbuffer";
    };

    /** Swapchain image acquired for the frame being recorded, imported in the frame graph. */
    struct PresentImage
    {
        /** Graph handle of the swapchain image (valid for the current frame only). */
        TextureHandle Texture = {};

        /** Image extent in pixels. */
        VkExtent2D Extent = {};

        /** Image format. */
        VkFormat Format = VK_FORMAT_UNDEFINED;

        /** Index of the image inside the swapchain. */
        std::uint32_t ImageIndex = 0U;
    };

    /**
     * Everything needed to present to one window: the surface, its swapchain, one acquire semaphore per frame slot
     * and the swapchain image state. A Renderer presents any number of targets in the same frame, with a single
     * submission: Renderer::BeginFrame acquires the main target, Renderer::AcquireTarget adds others (e.g. Dear ImGui
     * secondary viewports). Resizes and out-of-date swapchains are handled here, before the next acquisition.
     */
    class LUVK_API PresentTarget
    {
        friend class Renderer;

        /** Owning context. */
        Context& m_Context;

        /** Surface presented to. */
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        /** True when the surface is destroyed with the target. */
        bool m_OwnsSurface = true;

        /** Debug name of the imported image. */
        std::string m_Name;

        /** Window swapchain (empty when creation failed). */
        std::optional<Swapchain> m_Swapchain;

        /** One acquire semaphore per frame slot; reused once the slot's previous submission completed. */
        std::vector<VkSemaphore> m_AcquireSemaphores;

        /** Swapchain image state per frame slot (reset to UNDEFINED at every acquisition). */
        std::vector<ResourceState> m_States;

        /** Image acquired for each frame slot. */
        std::vector<std::uint32_t> m_AcquiredImages;

        /** Extent requested through Resize, applied before the next acquisition. */
        VkExtent2D m_PendingExtent = {};

        /** Set when the swapchain must be recreated before the next acquisition. */
        bool m_NeedsRecreate = false;

    public:
        /** Constructor; creates the swapchain (an initially zero-sized surface is created lazily). */
        PresentTarget(Context& Ctx, const PresentTargetDesc& Desc);

        /** Destructor; waits for the GPU and presentation engine, then destroys the swapchain and owned surface. */
        ~PresentTarget();

        /** Non-copyable. */
        PresentTarget(const PresentTarget&) = delete;

        /** Non-copyable. */
        PresentTarget& operator=(const PresentTarget&) = delete;

        /** Requests a new size; the swapchain is recreated before the next acquisition. */
        void Resize(VkExtent2D NewExtent) noexcept;

        /** Current swapchain extent in pixels ({0, 0} while not renderable). */
        [[nodiscard]] VkExtent2D Extent() const noexcept;

        /** Swapchain image format (VK_FORMAT_UNDEFINED while not renderable). */
        [[nodiscard]] VkFormat Format() const noexcept;

        /** Composite alpha mode selected for the surface. */
        [[nodiscard]] VkCompositeAlphaFlagBitsKHR CompositeAlpha() const noexcept;

        /** True when a swapchain exists. */
        [[nodiscard]] bool IsRenderable() const noexcept { return m_Swapchain.has_value() && m_Swapchain->IsValid(); }

    private:
        /** Recreates the swapchain if requested; false when nothing can be presented right now (e.g. minimised). */
        bool EnsureSwapchain();

        /** Acquires the next image for FrameSlot and imports it in Graph; std::nullopt when not presentable now. */
        [[nodiscard]] std::optional<PresentImage> Acquire(RenderGraph& Graph, std::uint32_t FrameSlot);

        /** Imports the image already acquired for FrameSlot again (after the frame graph was reset). */
        [[nodiscard]] TextureHandle Reimport(RenderGraph& Graph, std::uint32_t FrameSlot);

        /** Semaphore wait of FrameSlot's acquisition at the stage the graph first used the image. */
        [[nodiscard]] VkSemaphoreSubmitInfo AcquireWait(const RenderGraph& Graph, TextureHandle Texture, std::uint32_t FrameSlot) const;

        /** Semaphore signalled by the frame submission and waited by the presentation of FrameSlot's image. */
        [[nodiscard]] VkSemaphoreSubmitInfo RenderFinishedSignal(std::uint32_t FrameSlot) const;

        /** Blocks until the previous presentation of FrameSlot's image released its render-finished semaphore. */
        void WaitForPresent(std::uint32_t FrameSlot);

        /** Presents FrameSlot's image; marks the swapchain for recreation when it is no longer optimal. */
        void Present(std::uint32_t FrameSlot);

        /** Destroys the acquire semaphores. */
        void DestroySemaphores() noexcept;
    };
}
