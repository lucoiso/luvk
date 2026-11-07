// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "ImGuiRenderer.hpp"
#include "ImGuiRendererInternals.hpp"
#include <cstdio>
#include <luvk/Core/Context.hpp>
#include <luvk/Core/Error.hpp>
#include "ImGuiViewportWindow.hpp"

namespace luvk::imgui::detail
{
    void ImGuiCreateViewportWindow(ImGuiViewport* Viewport)
    {
        if (ImGuiRenderer* const Backend = CurrentBackend(); Backend != nullptr)
        {
            Backend->CreateViewportWindow(Viewport);
        }
    }

    void ImGuiDestroyViewportWindow(ImGuiViewport* Viewport)
    {
        if (ImGuiRenderer* const Backend = CurrentBackend(); Backend != nullptr)
        {
            Backend->DestroyViewportWindow(Viewport);
        }
    }
}

void luvk::imgui::ImGuiRenderer::CreateViewportWindow(ImGuiViewport* Viewport)
{
    if (!m_Viewports || Viewport == nullptr)
    {
        return;
    }

    ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
    if (PlatformIO.Platform_CreateVkSurface == nullptr)
    {
        std::fprintf(stderr, "[luvk-imgui] The platform backend does not provide Platform_CreateVkSurface.\n");
        return;
    }

    VkSurfaceKHR   Surface = VK_NULL_HANDLE;
    const VkResult Result  = static_cast<VkResult>(
        PlatformIO.Platform_CreateVkSurface(Viewport, HandleToU64(m_Context.Instance()), nullptr, reinterpret_cast<ImU64*>(&Surface)));
    if (Result != VK_SUCCESS || Surface == VK_NULL_HANDLE)
    {
        std::fprintf(stderr, "[luvk-imgui] ImGui viewport surface creation failed: %s\n", ToString(Result));
        return;
    }

    try
    {
        // The window takes ownership of the surface (also when its construction throws).
        Viewport->RendererUserData = new ImGuiViewportWindow(m_Context, Viewport, Surface, m_FrameCount);
    }
    catch (const std::exception& Exception)
    {
        std::fprintf(stderr, "[luvk-imgui] ImGui viewport initialization failed: %s\n", Exception.what());
    }
}

void luvk::imgui::ImGuiRenderer::DestroyViewportWindow(ImGuiViewport* Viewport)
{
    if (Viewport == nullptr)
    {
        return;
    }

    delete static_cast<ImGuiViewportWindow*>(Viewport->RendererUserData);
    Viewport->RendererUserData = nullptr;
}
