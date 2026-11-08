// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <string>

namespace luvk
{
    class Context;
    class Renderer;
}

namespace luvk::frame
{
    class SceneRenderer;
}

namespace luvk::app
{
    class Camera;

    /**
     * Built-in diagnostics: frame rate, CPU/GPU timings, scene settings, feature list and resource usage. Draw() shows
     * them in their own "luvk" window (ApplicationDesc::ShowStatistics); the section methods draw into any window, so
     * an application can embed them in its own UI (Application::Diagnostics()).
     */
    class Statistics
    {
    public:
        /** Constructor. Scene is null for UI-only applications; Camera is null without a fly camera. */
        Statistics(Context& Ctx, Renderer& TargetRenderer, frame::SceneRenderer* Scene, Camera* Camera, std::string Title = "luvk");

        /** Adds the elapsed time since Start to the exponential moving average of CPU stage Index. */
        void RecordCpuTime(std::size_t Index, std::chrono::steady_clock::time_point Start);

        /** Draws every section in the standalone diagnostics window. */
        void Draw();

        /** Draws the frame rate, device, resolutions and UI backend lines. */
        void DrawSummary() const;

        /** Draws the scene settings (render scale, wireframe, camera); nothing without a scene. */
        void DrawSettings();

        /** Draws the feature list with each feature's toggle and OnGui; nothing without a scene. */
        void DrawFeatures();

        /** Draws the GPU and CPU timings. */
        void DrawProfiling() const;

        /** Draws the bindless, deletion queue and validation counters. */
        void DrawResources() const;

    private:
        /** Exponential moving average of one CPU frame stage, in milliseconds. */
        struct CpuTiming
        {
            /** Stage label shown in the statistics window. */
            const char* Name = "";

            /** Smoothed stage duration in milliseconds. */
            double Average = 0.0;
        };

        /** Owning context. */
        Context& m_Context;

        /** Renderer providing the GPU profiler and frame counters. */
        Renderer& m_Renderer;

        /** Optional scene: settings and features listed and edited by the UI. */
        frame::SceneRenderer* m_Scene;

        /** Optional fly camera. */
        Camera* m_Camera;

        /** Window title. */
        std::string m_Title;

        /** EMA timings of the Update/UI/Record/Submit CPU stages, in milliseconds. */
        std::array<CpuTiming, 4> m_CpuTimings;
    };
}
