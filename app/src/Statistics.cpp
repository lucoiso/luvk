// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/Statistics.hpp"
#include <algorithm>
#include <utility>
#include <imgui.h>
#include <luvk/Core/Bindless.hpp>
#include <luvk/Core/Context.hpp>
#include <luvk/Render/Renderer.hpp>
#include "luvk/App/Camera.hpp"
#include <luvk/Frame/SceneRenderer.hpp>

luvk::app::Statistics::Statistics(Context& Ctx, Renderer& TargetRenderer, frame::SceneRenderer* Scene, Camera* Camera, std::string Title)
    : m_Context(Ctx),
      m_Renderer(TargetRenderer),
      m_Scene(Scene),
      m_Camera(Camera),
      m_Title(std::move(Title)),
      m_CpuTimings({{{"Update", 0.0}, {"UI", 0.0}, {"Record", 0.0}, {"Submit + present", 0.0}}})
{
}

void luvk::app::Statistics::RecordCpuTime(const std::size_t Index, const std::chrono::steady_clock::time_point Start)
{
    if (Index >= std::size(m_CpuTimings))
    {
        return;
    }

    const double Milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - Start).count();
    double&      Average      = m_CpuTimings[Index].Average;
    Average                   = Average == 0.0 ? Milliseconds : Average + (Milliseconds - Average) * 0.1;
}

void luvk::app::Statistics::Draw()
{
    ImGui::SetNextWindowSize(ImVec2(380.F, 520.F), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(m_Title.c_str(), nullptr))
    {
        ImGui::End();
        return;
    }

    DrawSummary();

    if (m_Scene != nullptr && ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawSettings();
    }

    if (m_Scene != nullptr && ImGui::CollapsingHeader("Features", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawFeatures();
    }

    if (ImGui::CollapsingHeader("Profiling", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawProfiling();
    }

    if (ImGui::CollapsingHeader("Resources"))
    {
        DrawResources();
    }

    ImGui::End();
}

void luvk::app::Statistics::DrawSummary() const
{
    const ImGuiIO& IO = ImGui::GetIO();
    ImGui::Text("%.1f FPS (%.2f ms)", IO.Framerate, 1000.F / std::max(IO.Framerate, 0.001F));
    ImGui::TextDisabled("%s", m_Context.Limits().DeviceName);
    if (m_Scene != nullptr)
    {
        const frame::FrameSettings& Settings = m_Scene->Settings();
        ImGui::TextDisabled("Output %ux%u  Render %ux%u",
                            Settings.OutputExtent.Width,
                            Settings.OutputExtent.Height,
                            Settings.RenderExtent.Width,
                            Settings.RenderExtent.Height);
    }
    ImGui::TextDisabled("Dear ImGui %s - backend: luvk (bindless)", IMGUI_VERSION);
}

void luvk::app::Statistics::DrawSettings()
{
    if (m_Scene == nullptr)
    {
        return;
    }

    float Scale = m_Scene->RenderScale();
    if (ImGui::SliderFloat("Render scale", &Scale, 0.25F, 2.F, "%.2fx"))
    {
        m_Scene->SetRenderScale(Scale);
    }

    bool Wireframe = m_Scene->Settings().Wireframe;
    ImGui::BeginDisabled(!m_Scene->SupportsWireframe());
    if (ImGui::Checkbox("Wireframe", &Wireframe))
    {
        m_Scene->SetWireframe(Wireframe);
    }
    ImGui::EndDisabled();

    bool ReverseDepth = m_Scene->Settings().ReverseDepth;
    if (ImGui::Checkbox("Reverse depth", &ReverseDepth))
    {
        m_Scene->SetReverseDepth(ReverseDepth);
    }

    if (m_Camera != nullptr)
    {
        ImGui::DragFloat("Camera speed", &m_Camera->Speed, 0.1F, 0.1F, 200.F);
        ImGui::SliderFloat("Field of view", &m_Camera->FieldOfView, 20.F, 120.F, "%.0f deg");
        ImGui::TextDisabled("WASD/QE move, right mouse look, wheel speed");
    }
}

void luvk::app::Statistics::DrawFeatures()
{
    if (m_Scene == nullptr)
    {
        return;
    }

    for (const auto& Feature : m_Scene->Features().All())
    {
        ImGui::PushID(Feature.get());
        bool Enabled = Feature->IsEnabled();
        if (ImGui::Checkbox("##enabled", &Enabled))
        {
            Feature->SetEnabled(Enabled);
        }
        ImGui::SameLine();
        const std::string Label(Feature->Name());
        if (ImGui::TreeNode(Label.c_str()))
        {
            Feature->OnGui();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

void luvk::app::Statistics::DrawProfiling() const
{
    if (const GpuProfiler* const Profiler = m_Renderer.Profiler(); Profiler != nullptr && Profiler->IsSupported())
    {
        ImGui::Text("%-22s %7.3f ms", Profiler->Frame().Name.c_str(), Profiler->Frame().Average);
        ImGui::Separator();
        for (const PassTiming& Timing : Profiler->Results())
        {
            ImGui::Text("  %-20s %7.3f ms", Timing.Name.c_str(), Timing.Average);
        }
    }
    else
    {
        ImGui::TextDisabled("GPU timings unavailable");
    }

    if (const GpuProfiler* const Async = m_Renderer.AsyncProfiler(); Async != nullptr && Async->IsSupported() && !std::empty(Async->Results()))
    {
        ImGui::Separator();
        ImGui::Text("%-22s %7.3f ms", m_Context.HasAsyncCompute() ? "Async compute (GPU)" : "Async graph (GPU)", Async->Frame().Average);
        for (const PassTiming& Timing : Async->Results())
        {
            ImGui::Text("  %-20s %7.3f ms", Timing.Name.c_str(), Timing.Average);
        }
    }

    ImGui::Separator();
    for (const CpuTiming& Timing : m_CpuTimings)
    {
        ImGui::Text("CPU %-18s %7.3f ms", Timing.Name, Timing.Average);
    }
}

void luvk::app::Statistics::DrawResources() const
{
    const BindlessHeap& Heap = m_Context.Bindless();
    ImGui::Text("Live resources:    %lld", static_cast<long long>(m_Context.LiveResourceCount()));
    ImGui::Text("Pending deletions: %zu", m_Context.PendingDeletions());
    ImGui::Text("Bindless textures: %u / %u", Heap.InUse(BindlessSlot::SampledImage), Heap.Capacity(BindlessSlot::SampledImage));
    ImGui::Text("Bindless images:   %u / %u", Heap.InUse(BindlessSlot::StorageImage), Heap.Capacity(BindlessSlot::StorageImage));
    ImGui::Text("Bindless samplers: %u / %u", Heap.InUse(BindlessSlot::Sampler), Heap.Capacity(BindlessSlot::Sampler));
    ImGui::Text("Validation: %s   Present fences: %s",
                m_Context.IsValidationEnabled() ? "on" : "off",
                m_Context.SupportsPresentFences() ? "on" : "off");
    ImGui::Text("Validation errors: %u", m_Context.ValidationErrorCount());
}
