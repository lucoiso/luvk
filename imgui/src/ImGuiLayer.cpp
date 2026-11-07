// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/ImGui/ImGuiLayer.hpp"
#include <algorithm>
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <SDL3/SDL_video.h>
#include <imgui_impl_sdl3.h>
#include <luvk/Core/Context.hpp>
#include <luvk/Platform/Window.hpp>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Render/Renderer.hpp>
#include <luvk/Resources/Image.hpp>
#include "ImGuiRenderer.hpp"
#include "ImGuiViewportWindow.hpp"

luvk::imgui::ImGuiLayer::ImGuiLayer(Context& Ctx, luvk::Renderer& TargetRenderer, platform::Window& Window, const Options& Options)
    : m_Context(&Ctx),
      m_Renderer(&TargetRenderer),
      m_Window(Window.Native()),
      m_Options(Options),
      m_IniPath(Options.IniPath.string())
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& IO    = ImGui::GetIO();
    IO.IniFilename = std::empty(m_IniPath) ? nullptr : m_IniPath.c_str();
    IO.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (m_Options.Docking)
    {
        IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }
    if (m_Options.Viewports)
    {
        IO.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    }

    // Fonts follow the display scale (also when a window moves to another monitor). Sizes and paddings belong to
    // the application's style and are scaled by the app layer once the style is applied.
    IO.ConfigDpiScaleFonts           = true;
    IO.ConfigDpiScaleViewports       = true;
    ImGui::GetStyle().FontScaleDpi = DisplayScale();

    if (!ImGui_ImplSDL3_InitForVulkan(m_Window))
    {
        ImGui::DestroyContext();
        throw std::runtime_error("[luvk-imgui] ImGui_ImplSDL3_InitForVulkan failed.");
    }

    try
    {
        m_Backend  = std::make_unique<ImGuiRenderer>(Ctx, TargetRenderer.FramesInFlight(), m_Options.Viewports);
        m_Geometry = std::make_unique<ImGuiGeometry>();
    }
    catch (...)
    {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        throw;
    }

    m_Initialized = true;
}

luvk::imgui::ImGuiLayer::~ImGuiLayer()
{
    if (!m_Initialized)
    {
        return;
    }

    try
    {
        ImGui::DestroyPlatformWindows();
    }
    catch (const std::exception& Exception)
    {
        std::fprintf(stderr, "[luvk-imgui] ImGui platform shutdown error: %s\n", Exception.what());
    }

    m_Backend.reset();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    m_Initialized = false;
}

void luvk::imgui::ImGuiLayer::ProcessEvent(const SDL_Event& Event)
{
    ImGui_ImplSDL3_ProcessEvent(&Event);
}

void luvk::imgui::ImGuiLayer::NewFrame()
{
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void luvk::imgui::ImGuiLayer::Render()
{
    ImGui::Render();

    // Creating, moving and destroying the OS windows is CPU-only work: doing it here, every UI frame, keeps Dear
    // ImGui consistent even when no GPU frame can be rendered (out-of-date swapchain, resize in progress...).
    if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
    {
        ImGui::UpdatePlatformWindows();
    }
}

void luvk::imgui::ImGuiLayer::PrepareFrame(FrameAllocator& Allocator)
{
    m_Backend->UpdateTextures();

    const ImDrawData* const DrawData = ImGui::GetDrawData();
    *m_Geometry                      = DrawData != nullptr ? m_Backend->UploadDrawData(*DrawData, Allocator) : ImGuiGeometry{};

    const ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
    for (int Index = 1; Index < PlatformIO.Viewports.Size; ++Index)
    {
        ImGuiViewport* const Viewport = PlatformIO.Viewports[Index];
        auto* const          Window   = static_cast<ImGuiViewportWindow*>(Viewport->RendererUserData);
        if (Window != nullptr && Viewport->DrawData != nullptr && (Viewport->Flags & ImGuiViewportFlags_IsMinimized) == 0)
        {
            Window->SyncSize();
            Window->Geometry() = m_Backend->UploadDrawData(*Viewport->DrawData, Allocator);
        }
    }
}

void luvk::imgui::ImGuiLayer::UseLiveImages(RenderGraph& Graph, PassBuilder& Pass) const
{
    for (Image* const Texture : m_Backend->LiveImages())
    {
        if (Texture != nullptr && *Texture)
        {
            Pass.Use(Graph.Import(*Texture), Access::ShaderSampled);
        }
    }
}

void luvk::imgui::ImGuiLayer::AddRenderPass(RenderGraph& Graph,
                                           const TextureHandle Backbuffer,
                                           const VkFormat Format,
                                           const LoadOp Load,
                                           const VkClearColorValue Clear)
{
    Graph.AddPass("ImGui",
                  PassType::Graphics,
                  [&](PassBuilder& Pass)
                  {
                      Pass.ColorAttachment(Backbuffer, Load, Clear);
                      UseLiveImages(Graph, Pass);
                  },
                  [this, Backbuffer, Format](CommandList& Cmd)
                  {
                      if (const ImDrawData* const DrawData = ImGui::GetDrawData(); DrawData != nullptr)
                      {
                          const GraphTexture& Target = Cmd.ResolveTexture(Backbuffer);
                          m_Backend->RenderDrawData(Cmd,
                                                    *DrawData,
                                                    *m_Geometry,
                                                    VkExtent2D{.width = Target.Extent.width, .height = Target.Extent.height},
                                                    Format);
                      }
                  });
}

void luvk::imgui::ImGuiLayer::AddViewportPasses(RenderGraph& Graph)
{
    if (!m_Options.Viewports)
    {
        return;
    }

    VkClearColorValue Clear{};
    std::copy(std::begin(m_Options.ViewportClearColor), std::end(m_Options.ViewportClearColor), std::begin(Clear.float32));

    const ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
    for (int Index = 1; Index < PlatformIO.Viewports.Size; ++Index)
    {
        ImGuiViewport* const Viewport = PlatformIO.Viewports[Index];
        auto* const          Window   = static_cast<ImGuiViewportWindow*>(Viewport->RendererUserData);
        if (Window == nullptr || Viewport->DrawData == nullptr || (Viewport->Flags & ImGuiViewportFlags_IsMinimized) != 0)
        {
            continue;
        }

        const std::optional<PresentImage> Image = m_Renderer->AcquireTarget(Window->Target());
        if (!Image)
        {
            continue;
        }

        const LoadOp Load = (Viewport->Flags & ImGuiViewportFlags_NoRendererClear) != 0 ? LoadOp::DontCare : LoadOp::Clear;
        Graph.AddPass("ImGui viewport",
                      PassType::Graphics,
                      [&](PassBuilder& Pass)
                      {
                          Pass.ColorAttachment(Image->Texture, Load, Clear);
                          UseLiveImages(Graph, Pass);
                      },
                      [this, Window, DrawData = Viewport->DrawData, Extent = Image->Extent, Format = Image->Format](CommandList& Cmd)
                      {
                          m_Backend->RenderDrawData(Cmd, *DrawData, Window->Geometry(), Extent, Format);
                      });
    }
}

ImTextureID luvk::imgui::ImGuiLayer::AddTexture(Image& Texture)
{
    return m_Backend->AddTexture(Texture);
}

void luvk::imgui::ImGuiLayer::RemoveTexture(const Image& Texture)
{
    m_Backend->RemoveTexture(Texture);
}

float luvk::imgui::ImGuiLayer::DisplayScale() const noexcept
{
    const float Scale = SDL_GetWindowDisplayScale(m_Window);
    return Scale > 0.F ? Scale : 1.F;
}
