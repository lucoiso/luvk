// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <imgui.h>
#include <luvk/Render/PresentTarget.hpp>
#include "ImGuiRenderer.hpp"

namespace luvk
{
    class Context;
}

namespace luvk::imgui
{
    /**
     * Window state of one Dear ImGui secondary viewport (multi-window): a PresentTarget owning the viewport's
     * surface and swapchain, plus the geometry of the frame being recorded. It is presented by the main Renderer in
     * the same submission as the main window.
     */
    class ImGuiViewportWindow
    {
    public:
        /** Constructor. Creates the present target for Viewport on Surface (takes ownership of Surface). */
        ImGuiViewportWindow(Context& Ctx, ImGuiViewport* Viewport, VkSurfaceKHR Surface, std::uint32_t FramesInFlight);

        /** Present target of the viewport window. */
        [[nodiscard]] PresentTarget& Target() noexcept { return m_Target; }

        /** Geometry of the frame being recorded. */
        [[nodiscard]] ImGuiGeometry& Geometry() noexcept { return m_Geometry; }

        /** Resizes the swapchain when the viewport's framebuffer size (logical size x framebuffer scale) changed. */
        void SyncSize();

    private:
        /** Dear ImGui viewport this window belongs to. */
        ImGuiViewport* m_Viewport;

        /** Surface, swapchain and acquire semaphores of the viewport window. */
        PresentTarget m_Target;

        /** Geometry of the frame being recorded (per-frame memory). */
        ImGuiGeometry m_Geometry;

        /** Framebuffer size last requested from the target, in pixels. */
        VkExtent2D m_RequestedExtent = {};
    };
}
