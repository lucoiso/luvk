// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/Application.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <SDL3/SDL.h>
#include <luvk/Core/Diagnostics.hpp>
#include <luvk/Core/Vulkan.hpp>
#include <luvk/ImGui/ImGuiLayer.hpp>
#include <luvk/Platform/Window.hpp>
#include <luvk/Render/GpuProfiler.hpp>
#include "luvk/App/Statistics.hpp"

namespace
{
    /** Exit code of Run after a GPU crash. */
    constexpr int DeviceLostExitCode = 3;

    /** Reads a boolean environment variable: unset -> nullopt, "0"/"false"/"off"/"no" -> false, anything else -> true. */
    std::optional<bool> EnvironmentFlag(const char* const Name)
    {
        const char* const Value = std::getenv(Name);
        if (Value == nullptr)
        {
            return std::nullopt;
        }

        const std::string Text(Value);
        return !(Text == "0" || Text == "false" || Text == "FALSE" || Text == "off" || Text == "OFF" || Text == "no" || Text == "NO");
    }

    /** True for automated runs (LUVK_APP_MAX_FRAMES set): no modal dialog may block them. */
    bool IsAutomatedRun()
    {
        return std::getenv("LUVK_APP_MAX_FRAMES") != nullptr;
    }
}

bool luvk::app::Application::IsAutomated() noexcept
{
    return IsAutomatedRun();
}

luvk::app::Application::Application(ApplicationDesc Desc)
    : m_Desc(std::move(Desc))
{
    // Environment overrides, so validation and diagnostics can be switched on for any build without recompiling.
    if (const std::optional<bool> Validation = EnvironmentFlag("LUVK_VALIDATION"))
    {
        m_Desc.EnableValidation = *Validation;
    }

    const auto Feature = [this](const char* const Name, bool& Target)
    {
        if (const std::optional<bool> Value = EnvironmentFlag(Name))
        {
            Target                  = *Value;
            m_Desc.EnableValidation = m_Desc.EnableValidation || *Value;
        }
    };
    Feature("LUVK_GPU_AV", m_Desc.Validation.GpuAssisted);
    Feature("LUVK_SYNC_VALIDATION", m_Desc.Validation.Synchronization);
    Feature("LUVK_DEBUG_PRINTF", m_Desc.Validation.DebugPrintf);
    Feature("LUVK_BEST_PRACTICES", m_Desc.Validation.BestPractices);

    if (const std::optional<bool> Diagnostics = EnvironmentFlag("LUVK_DIAGNOSTICS"))
    {
        m_Desc.EnableDiagnostics = *Diagnostics;

        // LUVK_DIAGNOSTICS=2 also keeps the breadcrumbs in device-coherent memory (hang-proof, but see DiagnosticsDesc).
        if (const char* const Level = std::getenv("LUVK_DIAGNOSTICS"); Level != nullptr && std::string_view(Level) == "2")
        {
            m_Desc.Diagnostics.DeviceCoherentMemory = true;
        }
    }

    if (EnvironmentFlag("LUVK_ROBUSTNESS").value_or(false))
    {
        m_Desc.Diagnostics.RobustBufferAccess2 = true;
        m_Desc.Diagnostics.RobustImageAccess2  = true;
        m_Desc.Diagnostics.NullDescriptor      = true;
    }
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
        if (m_Context && m_Context->IsDeviceLost())
        {
            // A GPU crash: report it instead of hanging; every wait of the teardown below returns at once.
            std::fprintf(stderr, "[luvk-app] GPU device lost: %s\n", Exception.what());
            ExitCode = HandleDeviceLost();
        }
        else
        {
            std::fprintf(stderr, "[luvk-app] Fatal error: %s\n", Exception.what());
            if (!IsAutomatedRun())
            {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, m_Desc.Title.c_str(), Exception.what(), m_Window ? m_Window->Native() : nullptr);
            }
            ExitCode = 1;
        }
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
            if (m_Context->ValidationErrorCount() > 0U && ExitCode == 0)
            {
                ExitCode = 2;
            }
        }
    }
    catch (const std::exception& Exception)
    {
        std::fprintf(stderr, "[luvk-app] Error during shutdown: %s\n", Exception.what());
        if (m_Context && m_Context->IsDeviceLost())
        {
            if (ExitCode != DeviceLostExitCode)
            {
                ExitCode = HandleDeviceLost();
            }
        }
        else
        {
            ExitCode = 1;
        }
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
        m_DataDirectory = !std::empty(m_Desc.DataDirectory) ? m_Desc.DataDirectory
                                                             : platform::UserDataDirectory(m_Desc.Organization, std::empty(m_Desc.AppName) ? m_Desc.Title : m_Desc.AppName);
    }

    m_Window = std::make_unique<platform::Window>(platform::WindowDesc{.Title       = m_Desc.Title,
                                                                        .Width       = m_Desc.Width,
                                                                        .Height      = m_Desc.Height,
                                                                        .Borderless  = m_Desc.Borderless,
                                                                        .Transparent = m_Desc.Transparent});

    std::vector<std::shared_ptr<Module>> Modules = m_Desc.Modules;
    if (m_Desc.EnableDiagnostics && std::ranges::none_of(Modules,
                                                         [](const std::shared_ptr<Module>& It)
                                                         {
                                                             return std::dynamic_pointer_cast<DiagnosticsModule>(It) != nullptr;
                                                         }))
    {
        Modules.push_back(std::make_shared<DiagnosticsModule>(m_Desc.Diagnostics));
    }

    m_Context = std::make_unique<Context>(ContextDesc{.ApplicationName      = m_Desc.Title,
                                                      .EnableValidation     = m_Desc.EnableValidation,
                                                      .InstanceExtensions   = platform::Window::VulkanInstanceExtensions(),
                                                      .CreateSurface        = [this](const VkInstance Instance)
                                                      {
                                                          return m_Window->CreateSurface(Instance);
                                                      },
                                                      .Modules              = std::move(Modules),
                                                      .PipelineCachePath    = std::empty(m_DataDirectory) ? std::filesystem::path{}
                                                                                                          : m_DataDirectory / "pipeline_cache.bin",
                                                      .Validation           = m_Desc.Validation,
                                                      .ExtraQueues          = m_Desc.ExtraQueues,
                                                      .CrashReportDirectory = CrashReportDirectory()});

    const bool       WantsTransparency = m_Desc.Transparent && !m_Desc.RenderScene;
    const Extent2D   WindowExtent      = m_Window->PixelSize();
    SwapchainDesc    Swapchain         = m_Desc.Swapchain;
    Swapchain.Surface                  = VK_NULL_HANDLE;
    Swapchain.PreferredPresentModes    = m_Desc.VSync ? std::vector{PresentMode::Fifo} : std::vector{PresentMode::Mailbox, PresentMode::Immediate, PresentMode::Fifo};
    Swapchain.Extent                   = {std::max(1U, WindowExtent.Width), std::max(1U, WindowExtent.Height)};
    if (WantsTransparency)
    {
        Swapchain.PreferredCompositeAlpha = CompositeAlpha::PreMultiplied;
    }

    m_Renderer = std::make_unique<Renderer>(*m_Context, RendererDesc{.Swapchain = Swapchain, .GpuTimings = m_Desc.GpuTimings});

    // LUVK_PASS_TIMINGS=0 keeps only the frame timestamps, to measure what ~two timestamps per pass cost.
    if (!EnvironmentFlag("LUVK_PASS_TIMINGS").value_or(true))
    {
        for (GpuProfiler* const Profiler : {m_Renderer->Profiler(), m_Renderer->AsyncProfiler()})
        {
            if (Profiler != nullptr)
            {
                Profiler->SetPassTimings(false);
            }
        }
    }

    m_TransparentActive = WantsTransparency && m_Window->IsTransparent() &&
                          m_Renderer->MainTarget().CompositeAlpha() != CompositeAlpha::Opaque;
    if (m_Desc.Transparent && !m_TransparentActive)
    {
        std::fputs(m_Desc.RenderScene ? "[luvk-app] Transparent windows are only supported with RenderScene = false; staying opaque.\n"
                                      : "[luvk-app] The window or surface does not support transparency; the window will stay opaque.\n",
                   stderr);
    }

    m_LinearSampler = Sampler(*m_Context,
                              {.AddressU = AddressMode::ClampToEdge,
                               .AddressV = AddressMode::ClampToEdge,
                               .AddressW = AddressMode::ClampToEdge,
                               .Name     = "luvk-app linear clamp"});

    if (m_Desc.RenderScene)
    {
        m_Scene = std::make_unique<frame::SceneRenderer>(*m_Context, m_LinearSampler, m_Desc.RenderScale);
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

std::filesystem::path luvk::app::Application::CrashReportDirectory() const
{
    return std::empty(m_DataDirectory) ? std::filesystem::path{} : m_DataDirectory / "crash-reports";
}

int luvk::app::Application::HandleDeviceLost() noexcept
{
    try
    {
        const DeviceLostReport* Report = m_Context->LastDeviceLostReport();
        if (Report == nullptr)
        {
            // The loss was observed outside a checked call (e.g. by third-party code): build the report now.
            m_Context->ReportDeviceLost("Application::Run");
            Report = m_Context->LastDeviceLostReport();
        }

        static const DeviceLostReport Empty{.Summary = "The GPU device was lost (VK_ERROR_DEVICE_LOST)."};
        const DeviceLostReport&       Current = Report != nullptr ? *Report : Empty;

        try
        {
            OnDeviceLost(Current);
        }
        catch (const std::exception& Exception)
        {
            std::fprintf(stderr, "[luvk-app] OnDeviceLost threw: %s\n", Exception.what());
        }

        if (m_Desc.ShowCrashDialog && !IsAutomatedRun())
        {
            const std::string Title = m_Desc.Title + " - GPU crash";
            SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, Title.c_str(), Current.Summary.c_str(), m_Window ? m_Window->Native() : nullptr);
        }
    }
    catch (...)
    {
    }

    return DeviceLostExitCode;
}

luvk::frame::SceneRenderer& luvk::app::Application::Scene() const
{
    if (!m_Scene)
    {
        throw std::logic_error("[luvk-app] The scene renderer only exists with ApplicationDesc::RenderScene = true.");
    }
    return *m_Scene;
}
