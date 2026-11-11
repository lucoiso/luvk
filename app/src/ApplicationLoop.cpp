// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/Application.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
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
        const bool        SceneHover = IsMouseOverSceneViewport();
        const bool        Swallowed  = !SceneHover && ((IsMouse && ImGui::GetIO().WantCaptureMouse) || (IsKey && ImGui::GetIO().WantCaptureKeyboard));

        switch (Event.type)
        {
        case SDL_EVENT_QUIT:
            m_Running = m_Running && !OnCloseRequested();
            break;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            if (Source == MainWindow)
            {
                m_Running = m_Running && !OnCloseRequested();
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
    using Clock = std::chrono::steady_clock;
    const auto Elapsed = [](const Clock::time_point From, const Clock::time_point To) { return std::chrono::duration<double, std::milli>(To - From).count(); };
    const auto Stage   = [this](const CpuStage Which) -> double& { return m_CpuStageTimes[static_cast<std::size_t>(Which)]; };

    auto Start = Clock::now();

    const ImGuiIO& IO = ImGui::GetIO();
    const bool     SceneHover = IsMouseOverSceneViewport();
    if (IsCameraEnabled() && ((!IO.WantCaptureMouse && !IO.WantCaptureKeyboard) || SceneHover))
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
    auto Now = Clock::now();
    Stage(CpuStage::Update) = Elapsed(Start, Now);

    Start = Now;
    m_ImGui->NewFrame();
    DrawUserInterface();
    m_ImGui->Render();
    if (m_Statistics)
    {
        m_Statistics->RecordCpuTime(1U, Start);
    }
    Now = Clock::now();
    Stage(CpuStage::Interface) = Elapsed(Start, Now);

    // Waiting for the frame slot (fence) and the swapchain image is not CPU work, but it is where a GPU-bound frame
    // spends its idle time: a benchmark reports it separately from the stages that do work.
    Start = Now;
    const std::optional<Frame> Current = m_Renderer->BeginFrame();
    Now = Clock::now();
    Stage(CpuStage::Acquire) = Elapsed(Start, Now);
    if (!Current)
    {
        return;
    }

    Start = Now;
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
    Now = Clock::now();
    Stage(CpuStage::Record) = Elapsed(Start, Now);

    Start = Now;
    m_Renderer->EndFrame(*Current);
    if (m_Statistics)
    {
        m_Statistics->RecordCpuTime(3U, Start);
    }
    Stage(CpuStage::Submit) = Elapsed(Start, Clock::now());
}

void luvk::app::Application::RecordFrame(const Frame& Current, const float DeltaTime)
{
    m_ImGui->PrepareFrame(Current.Allocator);

    if (m_Scene)
    {
        Rect2D   Viewport{};
        Extent2D ViewportExtent = Current.Extent;
        if (m_SceneViewportEnabled)
        {
            const float X0 = std::clamp(m_SceneViewport[0], 0.F, 1.F);
            const float Y0 = std::clamp(m_SceneViewport[1], 0.F, 1.F);
            const float X1 = std::clamp(m_SceneViewport[0] + m_SceneViewport[2], X0, 1.F);
            const float Y1 = std::clamp(m_SceneViewport[1] + m_SceneViewport[3], Y0, 1.F);

            Viewport = Rect2D{.Offset = {static_cast<std::int32_t>(std::lround(X0 * static_cast<float>(Current.Extent.Width))),
                                         static_cast<std::int32_t>(std::lround(Y0 * static_cast<float>(Current.Extent.Height)))},
                              .Extent = {static_cast<std::uint32_t>(std::lround((X1 - X0) * static_cast<float>(Current.Extent.Width))),
                                         static_cast<std::uint32_t>(std::lround((Y1 - Y0) * static_cast<float>(Current.Extent.Height)))}};
            ViewportExtent = Viewport.Extent;
        }

        const float Aspect = static_cast<float>(ViewportExtent.Width) / static_cast<float>(std::max(1U, ViewportExtent.Height));

        m_Camera.ReverseDepth = m_Scene->Settings().ReverseDepth;
        m_View                = m_Camera.MakeView(Aspect);
        if (m_ProjectionJitter.x != 0.F || m_ProjectionJitter.y != 0.F)
        {
            const float JitterX = 2.F * m_ProjectionJitter.x / static_cast<float>(std::max(1U, ViewportExtent.Width));
            const float JitterY = -2.F * m_ProjectionJitter.y / static_cast<float>(std::max(1U, ViewportExtent.Height));
            m_View.Projection[2][0] += JitterX;
            m_View.Projection[2][1] += JitterY;
            m_View.ViewProjection        = m_View.Projection * m_View.View;
            m_View.InverseViewProjection = glm::inverse(m_View.ViewProjection);
        }

        m_Scene->AddPasses(frame::SceneFrame{.Graph = Current.Graph,
                                      .Allocator    = Current.Allocator,
                                      .Backbuffer   = Current.Backbuffer,
                                      .OutputExtent = Current.Extent,
                                      .Viewport     = Viewport,
                                      .OutputFormat = Current.Format,
                                      .View         = IsCameraEnabled() ? &m_View : nullptr,
                                      .DeltaTime    = DeltaTime,
                                      .ProjectionJitter = m_ProjectionJitter,
                                      .Time         = m_Time,
                                      .FrameNumber  = Current.Number,
                                      .AsyncGraph   = &Current.AsyncGraph},
                           [this](frame::FeatureContext& Frame)
                           {
                               OnRender(Frame);
                           });

        m_ImGui->AddRenderPass(Current.Graph, Current.Backbuffer, Current.Format, LoadOp::Load);
    }
    else
    {
        ClearColorValue Clear{};
        std::copy(std::begin(m_Desc.BackgroundColor), std::end(m_Desc.BackgroundColor), std::begin(Clear.Float));

        // A premultiplied surface expects RGB = color * alpha, so an alpha-0 clear must also have RGB 0.
        if (m_TransparentActive && m_Renderer->MainTarget().CompositeAlpha() == CompositeAlpha::PreMultiplied)
        {
            for (std::size_t Index = 0U; Index < 3U; ++Index)
            {
                Clear.Float[Index] *= Clear.Float[3];
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

void luvk::app::Application::SetSceneViewport(const float X, const float Y, const float Width, const float Height) noexcept
{
    const float X0 = std::clamp(X, 0.F, 1.F);
    const float Y0 = std::clamp(Y, 0.F, 1.F);
    const float X1 = std::clamp(X + Width, X0, 1.F);
    const float Y1 = std::clamp(Y + Height, Y0, 1.F);

    m_SceneViewport        = {X0, Y0, X1 - X0, Y1 - Y0};
    m_SceneViewportEnabled = (X1 - X0) > 0.F && (Y1 - Y0) > 0.F;
    RequestRedraw();
}

void luvk::app::Application::ClearSceneViewport() noexcept
{
    m_SceneViewportEnabled = false;
    RequestRedraw();
}

bool luvk::app::Application::IsMouseOverSceneViewport() const noexcept
{
    if (!m_SceneViewportEnabled || !m_ImGui)
    {
        return false;
    }

    const ImVec2 Display = ImGui::GetIO().DisplaySize;
    const ImVec2 Mouse   = ImGui::GetIO().MousePos;

    if (Display.x <= 0.F || Display.y <= 0.F)
    {
        return false;
    }

    const float X0 = m_SceneViewport[0] * Display.x;
    const float Y0 = m_SceneViewport[1] * Display.y;
    const float X1 = (m_SceneViewport[0] + m_SceneViewport[2]) * Display.x;
    const float Y1 = (m_SceneViewport[1] + m_SceneViewport[3]) * Display.y;

    return Mouse.x >= X0 && Mouse.x < X1 && Mouse.y >= Y0 && Mouse.y < Y1;
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

bool luvk::app::Application::OnCloseRequested()
{
    return true;
}

void luvk::app::Application::OnUpdate(const float)
{
}

void luvk::app::Application::OnGui()
{
}

void luvk::app::Application::OnRender(frame::FeatureContext&)
{
}

void luvk::app::Application::OnDeviceLost(const DeviceLostReport&)
{
}
