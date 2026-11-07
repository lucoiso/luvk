// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/Application.hpp"
#include <algorithm>
#include <chrono>
#include <optional>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <luvk/ImGui/ImGuiLayer.hpp>
#include <luvk/Platform/Window.hpp>
#include "luvk/App/Statistics.hpp"

bool luvk::app::Application::PumpEvents()
{
    m_Input.BeginFrame();

    SDL_Window* const MainWindow = m_Window->Native();

    SDL_Event Event;
    ++m_IdleFrames;
    while (SDL_PollEvent(&Event))
    {
        m_IdleFrames = 0U;
        m_ImGui->ProcessEvent(Event);

        // Events of secondary ImGui windows only concern ImGui; events of no window (quit, keymap...) are global.
        SDL_Window* const Source     = SDL_GetWindowFromEvent(&Event);
        const bool        FromMain   = Source == nullptr || Source == MainWindow;
        const bool        IsMouse    = Event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || Event.type == SDL_EVENT_MOUSE_MOTION || Event.type == SDL_EVENT_MOUSE_WHEEL;
        const bool        IsKey      = Event.type == SDL_EVENT_KEY_DOWN;
        const bool        Swallowed  = (IsMouse && ImGui::GetIO().WantCaptureMouse) || (IsKey && ImGui::GetIO().WantCaptureKeyboard);

        switch (Event.type)
        {
        case SDL_EVENT_QUIT:
            m_Running = false;
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (Source == MainWindow)
            {
                m_Running = false;
            }
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            if (Source == MainWindow)
            {
                m_Renderer->Resize({static_cast<std::uint32_t>(Event.window.data1), static_cast<std::uint32_t>(Event.window.data2)});
            }
            break;
        case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            if (Source == MainWindow && m_Desc.ScaleUiWithDisplay)
            {
                ApplyUiScale(m_Window->DisplayScale());
            }
            break;
        default:
            break;
        }

        if (FromMain && !Swallowed)
        {
            m_Input.ProcessEvent(Event);
            OnEvent(Event);
        }
    }

    return m_Running;
}

void luvk::app::Application::ApplyUiScale(const float Scale)
{
    if (Scale <= 0.F || Scale == m_UiScale)
    {
        return;
    }

    // Fonts follow the monitor scale inside ImGui (ConfigDpiScaleFonts); paddings, roundings and sizes do not.
    ImGui::GetStyle().ScaleAllSizes(Scale / m_UiScale);
    m_UiScale = Scale;
}

void luvk::app::Application::RenderFrame(const float DeltaTime)
{
    auto Start = std::chrono::steady_clock::now();

    const ImGuiIO& IO = ImGui::GetIO();
    if (IsCameraEnabled() && !IO.WantCaptureMouse && !IO.WantCaptureKeyboard)
    {
        m_Camera.Update(m_Input, DeltaTime);
    }

    OnUpdate(DeltaTime);
    if (m_Scene)
    {
        m_Scene->Update(DeltaTime);
    }
    if (m_Statistics)
    {
        m_Statistics->RecordCpuTime(0U, Start);
    }

    Start = std::chrono::steady_clock::now();
    m_ImGui->NewFrame();
    DrawUserInterface();
    m_ImGui->Render();
    if (m_Statistics)
    {
        m_Statistics->RecordCpuTime(1U, Start);
    }

    const std::optional<Frame> Current = m_Renderer->BeginFrame();
    if (!Current)
    {
        return;
    }

    Start = std::chrono::steady_clock::now();
    try
    {
        RecordFrame(*Current, DeltaTime);
    }
    catch (...)
    {
        // Keep the renderer consistent (acquired images presented) before the error reaches Run().
        m_Renderer->AbortFrame(*Current);
        throw;
    }
    if (m_Statistics)
    {
        m_Statistics->RecordCpuTime(2U, Start);
    }

    Start = std::chrono::steady_clock::now();
    m_Renderer->EndFrame(*Current);
    if (m_Statistics)
    {
        m_Statistics->RecordCpuTime(3U, Start);
    }
}

void luvk::app::Application::RecordFrame(const Frame& Current, const float DeltaTime)
{
    m_ImGui->PrepareFrame(Current.Allocator);

    if (m_Scene)
    {
        const float Aspect = static_cast<float>(Current.Extent.width) / static_cast<float>(std::max(1U, Current.Extent.height));
        m_Camera.ReverseDepth = m_Scene->Settings().ReverseDepth;
        m_View                = m_Camera.MakeView(Aspect);

        m_Scene->AddPasses(SceneFrame{.Graph        = Current.Graph,
                                      .Allocator    = Current.Allocator,
                                      .Backbuffer   = Current.Backbuffer,
                                      .OutputExtent = Current.Extent,
                                      .OutputFormat = Current.Format,
                                      .View         = IsCameraEnabled() ? &m_View : nullptr,
                                      .DeltaTime    = DeltaTime,
                                      .Time         = m_Time,
                                      .FrameNumber  = Current.Number},
                           [this](FeatureContext& Frame)
                           {
                               OnRender(Frame);
                           });

        m_ImGui->AddRenderPass(Current.Graph, Current.Backbuffer, Current.Format, LoadOp::Load);
    }
    else
    {
        VkClearColorValue Clear{};
        std::copy(std::begin(m_Desc.BackgroundColor), std::end(m_Desc.BackgroundColor), std::begin(Clear.float32));

        // A premultiplied surface expects RGB = color * alpha, so an alpha-0 clear must also have RGB 0.
        if (m_TransparentActive && m_Renderer->MainTarget().CompositeAlpha() == VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        {
            for (std::size_t Index = 0U; Index < 3U; ++Index)
            {
                Clear.float32[Index] *= Clear.float32[3];
            }
        }

        m_ImGui->AddRenderPass(Current.Graph, Current.Backbuffer, Current.Format, LoadOp::Clear, Clear);
    }

    // Secondary OS windows of Dear ImGui are presented in the same submission as the main window.
    m_ImGui->AddViewportPasses(Current.Graph);
}

void luvk::app::Application::DrawUserInterface()
{
    OnGui();

    if (m_Desc.ShowStatistics)
    {
        m_Statistics->Draw();
    }
}

void luvk::app::Application::RequestExit() noexcept
{
    m_Running = false;
}

void luvk::app::Application::RequestRedraw() noexcept
{
    m_RedrawRequested = true;
}

ImTextureID luvk::app::Application::UiTexture(Image& Texture)
{
    if (!m_ImGui)
    {
        throw std::logic_error("[luvk-app] UiTexture requires the ImGui layer.");
    }
    return m_ImGui->AddTexture(Texture);
}

void luvk::app::Application::ReleaseUiTexture(const Image& Texture)
{
    if (m_ImGui)
    {
        m_ImGui->RemoveTexture(Texture);
    }
}

bool luvk::app::Application::IsImGuiReady() const noexcept
{
    return m_ImGui != nullptr && m_ImGui->IsInitialized();
}

void luvk::app::Application::OnInit()
{
}

void luvk::app::Application::OnShutdown()
{
}

void luvk::app::Application::OnEvent(const SDL_Event&)
{
}

void luvk::app::Application::OnUpdate(const float)
{
}

void luvk::app::Application::OnGui()
{
}

void luvk::app::Application::OnRender(FeatureContext&)
{
}
