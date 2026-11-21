// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "ImGuiViewportWindow.hpp"
#include <algorithm>
#include <cmath>

namespace
{
    /** Viewport size in pixels: ImGui sizes are logical units, swapchains are created in pixels. */
    luvk::Extent2D FramebufferExtent(const ImGuiViewport& Viewport)
    {
        const float ScaleX = Viewport.FramebufferScale.x > 0.F ? Viewport.FramebufferScale.x : 1.F;
        const float ScaleY = Viewport.FramebufferScale.y > 0.F ? Viewport.FramebufferScale.y : 1.F;
        return {static_cast<std::uint32_t>(std::max(1.F, std::round(Viewport.Size.x * ScaleX))),
                static_cast<std::uint32_t>(std::max(1.F, std::round(Viewport.Size.y * ScaleY)))};
    }
}

luvk::imgui::ImGuiViewportWindow::ImGuiViewportWindow(Context& Ctx, ImGuiViewport* Viewport, const VkSurfaceKHR Surface, const std::uint32_t FramesInFlight)
    : m_Viewport(Viewport),
      m_Target(Ctx,
               PresentTargetDesc{.Surface        = Surface,
                                 .OwnsSurface    = true,
                                 .Swapchain      = {.Extent           = FramebufferExtent(*Viewport),
                                                    .PreferredFormats = {{Format::B8G8R8A8Unorm, ColorSpace::SrgbNonlinear},
                                                                         {Format::R8G8B8A8Unorm, ColorSpace::SrgbNonlinear}}},
                                 .FramesInFlight = FramesInFlight,
                                 .Name           = "ImGui viewport"}),
      m_RequestedExtent(FramebufferExtent(*Viewport))
{
}

void luvk::imgui::ImGuiViewportWindow::SyncSize()
{
    const Extent2D Extent = FramebufferExtent(*m_Viewport);
    if (Extent != m_RequestedExtent)
    {
        m_RequestedExtent = Extent;
        m_Target.Resize(Extent);
    }
}
