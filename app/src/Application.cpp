// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/Application.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <SDL3/SDL.h>
#include <luvk/ImGui/ImGuiLayer.hpp>
#include <luvk/Platform/Window.hpp>
#include "luvk/App/Statistics.hpp"

luvk::app::Application::Application(ApplicationDesc Desc)
    : m_Desc(std::move(Desc))
{
}

luvk::app::Application::~Application()
{
    Shutdown();
}

int luvk::app::Application::Run()
{
    int ExitCode = 0;

    try
    {
        Initialize();
        OnInit();
        m_UserInitialized = true;

        // The style belongs to the application (applied in OnInit): scale its sizes once it exists.
        if (m_Desc.ScaleUiWithDisplay)
        {
            ApplyUiScale(m_Window->DisplayScale());
        }

        const char* const   MaxFramesValue = std::getenv("LUVK_APP_MAX_FRAMES");
        const std::uint64_t MaxFrames      = MaxFramesValue != nullptr ? std::strtoull(MaxFramesValue, nullptr, 10) : 0U;

        auto Previous = std::chrono::steady_clock::now();
        while (m_Running && PumpEvents())
        {
            if (MaxFrames != 0U && m_Renderer->FrameNumber() >= MaxFrames)
            {
                break;
            }

            if (m_Window->IsMinimized())
            {
                SDL_WaitEventTimeout(nullptr, 50);
                Previous = std::chrono::steady_clock::now();
                continue;
            }

            // Two extra frames after the last input let hover/click states settle before the loop sleeps.
            if (m_Desc.LowPower && !m_RedrawRequested && m_IdleFrames > 2U)
            {
                SDL_WaitEventTimeout(nullptr, 500);
            }
            m_RedrawRequested = false;

            const auto  Now       = std::chrono::steady_clock::now();
            const float DeltaTime = std::clamp(std::chrono::duration<float>(Now - Previous).count(), 0.F, 0.1F);
            Previous              = Now;
            m_Time += DeltaTime;

            RenderFrame(DeltaTime);
        }
    }
    catch (const std::exception& Exception)
    {
        std::fprintf(stderr, "[luvk-app] Fatal error: %s\n", Exception.what());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, m_Desc.Title.c_str(), Exception.what(), m_Window ? m_Window->Native() : nullptr);
        ExitCode = 1;
    }

    try
    {
        if (m_Context)
        {
            m_Context->WaitIdle();
            if (m_UserInitialized)
            {
                OnShutdown();
            }

            std::printf("[luvk-app] %llu frames rendered, %u validation errors (validation %s).\n",
                        static_cast<unsigned long long>(m_Renderer ? m_Renderer->FrameNumber() : 0U),
                        m_Context->ValidationErrorCount(),
                        m_Context->IsValidationEnabled() ? "on" : "off");
            if (m_Context->ValidationErrorCount() > 0U)
            {
                ExitCode = 2;
            }
        }
    }
    catch (const std::exception& Exception)
    {
        std::fprintf(stderr, "[luvk-app] Error during shutdown: %s\n", Exception.what());
        ExitCode = 1;
    }

    // The UI goes now, while the subclass is still alive (Dear ImGui saves its layout and may still call into
    // subclass state). GPU objects wait for ~Application: the subclass's own Buffers/Images/features must be
    // released before the Context that owns their memory.
    ShutdownUi();
    return ExitCode;
}

void luvk::app::Application::Initialize()
{
    if (m_Desc.PersistState)
    {
        m_DataDirectory = platform::UserDataDirectory(m_Desc.Organization, std::empty(m_Desc.AppName) ? m_Desc.Title : m_Desc.AppName);
    }

    m_Window = std::make_unique<platform::Window>(platform::WindowDesc{.Title       = m_Desc.Title,
                                                                        .Width       = m_Desc.Width,
                                                                        .Height      = m_Desc.Height,
                                                                        .Borderless  = m_Desc.Borderless,
                                                                        .Transparent = m_Desc.Transparent});

    m_Context = std::make_unique<Context>(ContextDesc{.ApplicationName    = m_Desc.Title,
                                                      .EnableValidation   = m_Desc.EnableValidation,
                                                      .InstanceExtensions = platform::Window::VulkanInstanceExtensions(),
                                                      .CreateSurface      = [this](const VkInstance Instance)
                                                      {
                                                          return m_Window->CreateSurface(Instance);
                                                      },
                                                      .Modules           = m_Desc.Modules,
                                                      .PipelineCachePath = std::empty(m_DataDirectory) ? std::filesystem::path{}
                                                                                                       : m_DataDirectory / "pipeline_cache.bin"});

    const bool       WantsTransparency = m_Desc.Transparent && !m_Desc.RenderScene;
    const VkExtent2D WindowExtent      = m_Window->PixelSize();
    SwapchainDesc    Swapchain         = m_Desc.Swapchain;
    Swapchain.Surface                  = VK_NULL_HANDLE;
    Swapchain.PreferredPresentModes    = m_Desc.VSync ? std::vector{VK_PRESENT_MODE_FIFO_KHR}
                                                      : std::vector{VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR};
    Swapchain.Extent                   = {std::max(1U, WindowExtent.width), std::max(1U, WindowExtent.height)};
    if (WantsTransparency)
    {
        Swapchain.PreferredCompositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    }

    m_Renderer = std::make_unique<Renderer>(*m_Context, RendererDesc{.Swapchain = Swapchain, .GpuTimings = m_Desc.GpuTimings});

    m_TransparentActive = WantsTransparency && m_Window->IsTransparent() &&
                          m_Renderer->MainTarget().CompositeAlpha() != VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (m_Desc.Transparent && !m_TransparentActive)
    {
        std::fputs(m_Desc.RenderScene ? "[luvk-app] Transparent windows are only supported with RenderScene = false; staying opaque.\n"
                                      : "[luvk-app] The window or surface does not support transparency; the window will stay opaque.\n",
                   stderr);
    }

    m_LinearSampler = Sampler(*m_Context,
                              {.AddressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .AddressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .AddressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                               .Name     = "luvk-app linear clamp"});

    if (m_Desc.RenderScene)
    {
        m_Scene = std::make_unique<SceneRenderer>(*m_Context, m_LinearSampler, m_Desc.RenderScale);
    }

    m_ImGui = std::make_unique<luvk::imgui::ImGuiLayer>(*m_Context,
                                                        *m_Renderer,
                                                        *m_Window,
                                                        luvk::imgui::ImGuiLayer::Options{.Docking            = m_Desc.EnableDocking,
                                                                                         .Viewports          = m_Desc.EnableViewports,
                                                                                         .IniPath            = std::empty(m_DataDirectory)
                                                                                                                   ? std::filesystem::path{}
                                                                                                                   : m_DataDirectory / "imgui.ini",
                                                                                         .ViewportClearColor = m_Desc.BackgroundColor});

    m_Statistics = std::make_unique<Statistics>(*m_Context, *m_Renderer, m_Scene.get(), IsCameraEnabled() ? &m_Camera : nullptr);
}

void luvk::app::Application::ShutdownUi() noexcept
{
    try
    {
        if (m_Context)
        {
            m_Context->WaitIdle();
        }
    }
    catch (const std::exception& Exception)
    {
        std::fprintf(stderr, "[luvk-app] Error waiting for the device: %s\n", Exception.what());
    }

    m_Statistics.reset();
    m_ImGui.reset();
}

void luvk::app::Application::Shutdown() noexcept
{
    ShutdownUi();

    try
    {
        m_Scene.reset();
    }
    catch (const std::exception& Exception)
    {
        std::fprintf(stderr, "[luvk-app] Error detaching features: %s\n", Exception.what());
    }

    m_LinearSampler.Reset();
    m_Renderer.reset();
    m_Context.reset();
    m_Window.reset();
}

luvk::app::SceneRenderer& luvk::app::Application::Scene() const
{
    if (!m_Scene)
    {
        throw std::logic_error("[luvk-app] The scene renderer only exists with ApplicationDesc::RenderScene = true.");
    }
    return *m_Scene;
}
