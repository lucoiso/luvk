// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <imgui.h>
#include <luvk/Core/Context.hpp>
#include <luvk/Core/Module.hpp>
#include "luvk/App/Feature.hpp"
#include <luvk/Render/Renderer.hpp>
#include <luvk/Platform/Input.hpp>
#include <luvk/Platform/Window.hpp>
#include <luvk/Resources/Image.hpp>
#include <luvk/Resources/Sampler.hpp>
#include "luvk/App/Camera.hpp"
#include "luvk/App/SceneRenderer.hpp"
#include "luvk/App/Statistics.hpp"

union SDL_Event;

namespace luvk::imgui
{
    class ImGuiLayer;
}

namespace luvk::app
{
    /** Options describing the window, renderer, validation, persistence and UI behaviour of an Application. */
    struct ApplicationDesc
    {
        /** Window title, also passed to the Context as the Vulkan application name. */
        std::string Title = "luvk";

        /** Initial window width in logical units (scaled by the display scale). */
        std::uint32_t Width = 1280U;

        /** Initial window height in logical units (scaled by the display scale). */
        std::uint32_t Height = 720U;

        /** Creates the OS window without a native title bar/frame (useful for custom-drawn UIs). */
        bool Borderless = false;

        /** Requests a per-pixel transparent window (only meaningful with RenderScene = false). */
        bool Transparent = false;

#ifdef LUVK_APP_VALIDATION_DEFAULT

        /** Enables Vulkan validation layers and debug messaging when true. */
        bool EnableValidation = true;
#else

        /** Enables Vulkan validation layers and debug messaging when true. */
        bool EnableValidation = false;
#endif

        /** Collects GPU pass timings with the renderer's profiler when true. */
        bool GpuTimings = true;

        /** Shows the built-in "luvk" statistics window (the sections stay available through Diagnostics()). */
        bool ShowStatistics = true;

        /** Waits for the display refresh (FIFO). False prefers MAILBOX / IMMEDIATE: lower latency, more power. */
        bool VSync = true;

        /** Tool mode: once nothing changed for a couple of frames, sleeps until input arrives (or RequestRedraw()). */
        bool LowPower = false;

        /**
         * Renders the scene (SceneRenderer: SceneColor/SceneDepth, features, composite) under the UI.
         * When false the application is a pure Dear ImGui host: the backbuffer is cleared with BackgroundColor and
         * only the UI is drawn (EnableCamera and RenderScale are then ignored).
         */
        bool RenderScene = true;

        /** Creates and feeds a fly camera, publishing Slots::View on the blackboard (only with RenderScene). */
        bool EnableCamera = true;

        /** Enables Dear ImGui docking. */
        bool EnableDocking = true;

        /** Enables Dear ImGui multi-viewport (windows can leave the main OS window). */
        bool EnableViewports = true;

        /** Scales the UI style and fonts with the display scale (and follows monitor changes). */
        bool ScaleUiWithDisplay = true;

        /** Scene resolution relative to the window (only with RenderScene). */
        float RenderScale = 1.F;

        /** Backbuffer clear color used when RenderScene is false (also the clear color of secondary UI windows). */
        std::array<float, 4> BackgroundColor = {0.02F, 0.02F, 0.03F, 1.F};

        /** Extra device requirements (e.g. luvk::MeshShaderModule, luvk::RayTracingModule). */
        std::vector<std::shared_ptr<Module>> Modules;

        /** Main swapchain preferences (Surface, Extent and PreferredPresentModes are filled by the application, the latter
         *  from VSync). The default UNORM backbuffer lets the composite encode sRGB itself, so the UI stays correct. */
        SwapchainDesc Swapchain{.PreferredFormats = {{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                                                     {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};

        /** Organization folder of the per-user data directory (see luvk::platform::UserDataDirectory). */
        std::string Organization = "luvk";

        /** Application folder of the per-user data directory; empty uses Title. */
        std::string AppName;

        /** Persists the Dear ImGui layout (imgui.ini) and the Vulkan pipeline cache in the per-user data directory. */
        bool PersistState = true;
    };

    /**
     * Application skeleton: SDL3 window, luvk Context/Renderer, Dear ImGui rendered entirely through luvk
     * (no official ImGui Vulkan backend), an optional fly camera and an optional SceneRenderer.
     *
     * Responsibilities: the main loop, event routing, window/UI lifetime, persistence and teardown. Rendering of the
     * scene lives in SceneRenderer (Scene()), presentation in luvk::Renderer.
     *
     * Subclasses override the On* hooks and add features in OnInit(). The UI is torn down at the end of Run(), while
     * the subclass is still alive; GPU objects in ~Application, after the subclass released its own resources.
     */
    class Application
    {
    public:
        /** Constructor. Stores the configuration; startup happens in Run(). */
        explicit Application(ApplicationDesc Desc);

        /** Destructor. Releases the scene, renderer, context and window (and the UI if Run() did not). */
        virtual ~Application();

        /** Non-copyable. */
        Application(const Application&) = delete;

        /** Non-copyable. */
        Application& operator=(const Application&) = delete;

        /** Runs until the window closes, then tears the UI down. Returns the process exit code. */
        [[nodiscard]] int Run();

        /** Stops the main loop after the current frame; Run() then returns. */
        void RequestExit() noexcept;

        /** With ApplicationDesc::LowPower, renders the next frames even without input (e.g. while something animates). */
        void RequestRedraw() noexcept;

        /** Returns the Vulkan context (instance, device, queues, resources). */
        [[nodiscard]] luvk::Context& GetContext() const noexcept { return *m_Context; }

        /** Returns the renderer driving the frame submissions and presentation. */
        [[nodiscard]] luvk::Renderer& GetRenderer() const noexcept { return *m_Renderer; }

        /** Returns the platform window that hosts the main viewport (use Native() for the SDL handle). */
        [[nodiscard]] platform::Window& GetWindow() const noexcept { return *m_Window; }

        /** Returns the scene renderer (throws when ApplicationDesc::RenderScene is false). */
        [[nodiscard]] SceneRenderer& Scene() const;

        /** Shortcut for Scene().Features(). */
        [[nodiscard]] FeatureStack& Features() const { return Scene().Features(); }

        /** Returns the keyboard/mouse state fed by SDL events (main window only). */
        [[nodiscard]] platform::Input& GetInput() noexcept { return m_Input; }

        /** Returns the fly camera driven by the input state. */
        [[nodiscard]] luvk::app::Camera& GetCamera() noexcept { return m_Camera; }

        /** Built-in diagnostics, drawable inside any window of the application's UI. */
        [[nodiscard]] Statistics& Diagnostics() const noexcept { return *m_Statistics; }

        /** Returns the camera matrices of the current frame. */
        [[nodiscard]] const ViewData& View() const noexcept { return m_View; }

        /** Returns the shared linear clamp sampler. */
        [[nodiscard]] const Sampler& LinearSampler() const noexcept { return m_LinearSampler; }

        /** Returns the elapsed application time in seconds. */
        [[nodiscard]] double Time() const noexcept { return m_Time; }

        /** Exposes an Image (SAMPLED usage) to ImGui::Image through the luvk bindless heap (cheap to call every frame).
         *  The image is transitioned to a sampleable layout before the UI passes; call ReleaseUiTexture before
         *  destroying it. */
        [[nodiscard]] ImTextureID UiTexture(Image& Texture);

        /** Stops exposing an image previously registered with UiTexture. */
        void ReleaseUiTexture(const Image& Texture);

        /** True when Dear ImGui was initialized and the current frame may draw UI. */
        [[nodiscard]] bool IsImGuiReady() const noexcept;

        /** True when the application owns a fly camera and publishes Slots::View. */
        [[nodiscard]] bool IsCameraEnabled() const noexcept { return m_Desc.EnableCamera && m_Desc.RenderScene; }

        /** True when the application renders the scene under the UI. */
        [[nodiscard]] bool IsSceneEnabled() const noexcept { return m_Scene != nullptr; }

        /** True when a transparent window was requested AND both the window and the swapchain support it. */
        [[nodiscard]] bool IsTransparent() const noexcept { return m_TransparentActive; }

        /** Display scale the UI style is currently scaled by (1.0 when ScaleUiWithDisplay is off). */
        [[nodiscard]] float UiScale() const noexcept { return m_UiScale; }

        /** Per-user directory where the application state is persisted (empty when PersistState is off). */
        [[nodiscard]] const std::filesystem::path& DataDirectory() const noexcept { return m_DataDirectory; }

    protected:
        /** Called once after initialization, before the first frame; add features and apply the UI style here. */
        virtual void OnInit();

        /** Called once after the last frame, with the GPU idle and everything still alive. */
        virtual void OnShutdown();

        /** Called for every SDL event of the main window (or of no window, e.g. quit) not consumed by ImGui. */
        virtual void OnEvent(const SDL_Event& Event);

        /** Called every frame with the clamped delta time in seconds, before the features update. */
        virtual void OnUpdate(float DeltaTime);

        /** Called every frame inside the ImGui new-frame/render scope. */
        virtual void OnGui();

        /** Custom scene passes added after the Overlay features and before the Output one (only with RenderScene). */
        virtual void OnRender(FeatureContext& Frame);

    private:
        /** Creates the window, context, renderer, scene, sampler and ImGui backend. */
        void Initialize();

        /** Destroys the statistics window and the ImGui layer (saving the layout) after waiting for the GPU (idempotent). */
        void ShutdownUi() noexcept;

        /** Tears down the UI, the scene, the renderer, the context and the SDL window (idempotent). */
        void Shutdown() noexcept;

        /** Pumps SDL events into Input/ImGui, handles quit, resize and scale changes; returns whether to keep running. */
        bool PumpEvents();

        /** Updates camera and features, builds the UI, records and submits one frame. */
        void RenderFrame(float DeltaTime);

        /** Records the frame's passes (scene or clear, UI, secondary UI windows). */
        void RecordFrame(const Frame& Current, float DeltaTime);

        /** Scales the UI style sizes from the current UI scale to Scale. */
        void ApplyUiScale(float Scale);

        /** Draws subclasses' UI plus the optional built-in statistics window. */
        void DrawUserInterface();

        /** Configuration the application was constructed with. */
        ApplicationDesc m_Desc;

        /** Per-user directory of the persisted state (empty when PersistState is off or unavailable). */
        std::filesystem::path m_DataDirectory;

        /** SDL3 window; null until Initialize(). */
        std::unique_ptr<platform::Window> m_Window;

        /** False once a quit was requested, ending the main loop. */
        bool m_Running = true;

        /** True once OnInit() returned (OnShutdown() is only called then). */
        bool m_UserInitialized = false;

        /** True when transparency was requested and both the window and the surface accepted it. */
        bool m_TransparentActive = false;

        /** Display scale the style sizes are currently scaled by. */
        float m_UiScale = 1.F;

        /** Elapsed time since startup, in seconds. */
        double m_Time = 0.0;

        /** Vulkan instance, device, queues and resources owned by the application. */
        std::unique_ptr<Context> m_Context;

        /** Frame scheduling and presentation. */
        std::unique_ptr<Renderer> m_Renderer;

        /** Linear clamp sampler used by the composite and exposed to features. */
        Sampler m_LinearSampler;

        /** Scene renderer (null when RenderScene is false). */
        std::unique_ptr<SceneRenderer> m_Scene;

        /** Dear ImGui context and the luvk-based renderer backend. */
        std::unique_ptr<luvk::imgui::ImGuiLayer> m_ImGui;

        /** Built-in diagnostics (shown in their own window when ShowStatistics is set). */
        std::unique_ptr<Statistics> m_Statistics;

        /** Frames rendered since the last input event (LowPower idling). */
        std::uint32_t m_IdleFrames = 0U;

        /** Set by RequestRedraw; keeps LowPower from sleeping for the next frames. */
        bool m_RedrawRequested = false;

        /** Keyboard/mouse state accumulated from SDL events. */
        platform::Input m_Input;

        /** Fly camera driven by m_Input. */
        Camera m_Camera;

        /** Camera matrices of the current frame. */
        ViewData m_View;
    };
}
