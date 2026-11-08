// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "ExampleUi.hpp"
#include <algorithm>
#include <luvk/App/Application.hpp>
#include <luvk/App/DebugDraw.hpp>
#include <luvk/App/MeshFeature.hpp>
#include <luvk/Core/Context.hpp>
#include "MeshShaderCube.hpp"
#include "Particles.hpp"
#include "RayTracingPreview.hpp"
#include "ShaderPlayground.hpp"
#include "Skybox.hpp"
#include "Gallery.hpp"

Example::ExampleUi::ExampleUi(luvk::app::Application& App, const GalleryExhibits& Exhibits)
    : m_App(App),
      m_Exhibits(Exhibits),
      m_ShaderCode(ShaderPlayground::DefaultSource())
{
}

void Example::ExampleUi::Draw()
{
    const ImGuiViewport* const Main = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(Main->WorkPos.x + 12.F, Main->WorkPos.y + 12.F), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(440.F, Main->WorkSize.y - 24.F), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("luvk gallery"))
    {
        ImGui::TextDisabled("WASD/QE move, right mouse look, wheel changes the speed");

        if (ImGui::BeginTabBar("##gallery_tabs"))
        {
            if (ImGui::BeginTabItem("Scene"))
            {
                DrawSceneTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Ray Tracing"))
            {
                DrawRayTracingTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Shader Lab"))
            {
                DrawShaderLabTab();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Diagnostics"))
            {
                DrawDiagnosticsTab();
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    DrawFloatingWindow();

    if (m_ShowDemoWindow)
    {
        ImGui::ShowDemoWindow(&m_ShowDemoWindow);
    }
}

void Example::ExampleUi::DrawSceneTab()
{
    if (ImGui::CollapsingHeader("Exhibits", ImGuiTreeNodeFlags_DefaultOpen))
    {
        DrawExhibit(m_Exhibits.Meshes, "Meshes, materials and instancing", Gallery::Spheres);
        DrawExhibit(m_Exhibits.Cube, "Mesh shader cube", Gallery::MeshShaderCube);
        DrawExhibit(m_Exhibits.Fountain, "Particle fountain", Gallery::Fountain);
        DrawExhibit(m_Exhibits.RayTracing, "Ray traced screen", Gallery::RayTracingScreen);
        DrawExhibit(m_Exhibits.ShaderLab, "Shader Lab screen", Gallery::ShaderLabScreen);
        DrawExhibit(m_Exhibits.Sky, "Skybox", std::nullopt);
    }

    if (ImGui::CollapsingHeader("Debug draw", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Checkbox("Floor grid", &m_DrawGrid);
        ImGui::SameLine();
        ImGui::Checkbox("World axes", &m_DrawAxes);
        ImGui::SameLine();
        ImGui::Checkbox("Cube bounds", &m_DrawBounds);

        bool Frozen = m_FrozenFrustum.has_value();
        if (ImGui::Checkbox("Freeze camera frustum", &Frozen))
        {
            m_FrozenFrustum = Frozen ? std::optional<glm::mat4>(m_App.View().InverseViewProjection) : std::nullopt;
        }
        m_Exhibits.Debug.OnGui();
    }

    if (ImGui::CollapsingHeader("Lighting"))
    {
        m_Exhibits.Meshes.OnGui();
    }
}

void Example::ExampleUi::DrawExhibit(luvk::frame::RenderFeature& Feature, const char* const Label, const std::optional<glm::vec3>& Target)
{
    ImGui::PushID(&Feature);

    bool Enabled = Feature.IsEnabled();
    if (ImGui::Checkbox("##enabled", &Enabled))
    {
        Feature.SetEnabled(Enabled);
    }
    ImGui::SameLine();

    const bool Open = ImGui::TreeNode(Label);
    if (Target.has_value())
    {
        // Right-aligned "Go" button.
        const float ButtonWidth = ImGui::CalcTextSize("Go").x + ImGui::GetStyle().FramePadding.x * 2.F;
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.F, ImGui::GetContentRegionAvail().x - ButtonWidth));
        if (ImGui::SmallButton("Go"))
        {
            Focus(*Target);
        }
    }

    if (Open)
    {
        Feature.OnGui();
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void Example::ExampleUi::DrawRayTracingTab()
{
    RayTracingPreview& RayTracing = m_Exhibits.RayTracing;
    if (!RayTracing.IsAvailable())
    {
        ImGui::TextColored(ImVec4(1.F, 0.75F, 0.2F, 1.F), "Unavailable");
        ImGui::TextWrapped("%s", RayTracing.Status().c_str());
        return;
    }

    ImGui::TextWrapped("A Cornell box traced with hardware ray queries in a compute shader: one primary ray per pixel, a shadow "
                       "ray to the ceiling light and one reflection bounce. The image is traced into mip 0, its mip chain is "
                       "generated every frame and the gallery screen samples it.");
    ImGui::Separator();
    ImageFit(m_App.UiTexture(RayTracing.Output()), 400.F);
    RayTracing.OnGui();
}

void Example::ExampleUi::DrawShaderLabTab()
{
    if (ImGui::Button("Compile"))
    {
        m_CompileError = m_Exhibits.ShaderLab.Compile(m_ShaderCode);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset"))
    {
        m_ShaderCode   = std::string(ShaderPlayground::DefaultSource());
        m_CompileError = m_Exhibits.ShaderLab.Compile(m_ShaderCode);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Slang, compiled at runtime; shown on the right screen");

    if (!std::empty(m_CompileError))
    {
        ImGui::PushTextWrapPos();
        ImGui::TextColored(ImVec4(1.F, 0.35F, 0.35F, 1.F), "%s", m_CompileError.c_str());
        ImGui::PopTextWrapPos();
    }

    const float Preview = std::min(ImGui::GetContentRegionAvail().x, 220.F);
    ImGui::InputTextMultiline("##source",
                              m_ShaderCode.data(),
                              m_ShaderCode.capacity() + 1U,
                              ImVec2(-1.F, std::max(120.F, ImGui::GetContentRegionAvail().y - Preview - 12.F)),
                              ImGuiInputTextFlags_CallbackResize | ImGuiInputTextFlags_AllowTabInput,
                              [](ImGuiInputTextCallbackData* Data)
                              {
                                  if (Data->EventFlag == ImGuiInputTextFlags_CallbackResize)
                                  {
                                      auto* const Text = static_cast<std::string*>(Data->UserData);
                                      Text->resize(static_cast<std::size_t>(Data->BufTextLen));
                                      Data->Buf = Text->data();
                                  }
                                  return 0;
                              },
                              &m_ShaderCode);

    ImageFit(m_App.UiTexture(m_Exhibits.ShaderLab.Output()), Preview);
}

void Example::ExampleUi::DrawDiagnosticsTab()
{
    luvk::app::Statistics& Diagnostics = m_App.Diagnostics();
    Diagnostics.DrawSummary();

    if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Diagnostics.DrawSettings();
    }

    if (ImGui::CollapsingHeader("Profiling", ImGuiTreeNodeFlags_DefaultOpen))
    {
        Diagnostics.DrawProfiling();
    }

    if (ImGui::CollapsingHeader("Resources"))
    {
        Diagnostics.DrawResources();
    }

    if (ImGui::CollapsingHeader("Capabilities"))
    {
        const auto Status = [](const char* Name, const bool Available, const std::string& Detail)
        {
            ImGui::TextColored(Available ? ImVec4(0.4F, 0.9F, 0.4F, 1.F) : ImVec4(1.F, 0.75F, 0.2F, 1.F), "%s %s", Available ? "[ok] " : "[off]", Name);
            ImGui::TextWrapped("%s", Detail.c_str());
            ImGui::Separator();
        };

        const luvk::Context& Ctx = m_App.GetContext();
        Status("Mesh shading", m_Exhibits.Cube.IsAvailable(), m_Exhibits.Cube.Status());
        Status("Ray queries", m_Exhibits.RayTracing.IsAvailable(), m_Exhibits.RayTracing.Status());
        Status("Present fences", Ctx.SupportsPresentFences(), "VK_KHR/EXT_swapchain_maintenance1: exact present semaphore lifetimes.");
        Status("Validation layers", Ctx.IsValidationEnabled(), Ctx.IsValidationEnabled() ? "VK_LAYER_KHRONOS_validation active." : "Install the Vulkan SDK to enable validation.");
    }

    if (ImGui::CollapsingHeader("Windows"))
    {
        ImGui::Checkbox("Floating window (second OS window)", &m_ShowFloatingWindow);
        ImGui::Checkbox("Dear ImGui demo window", &m_ShowDemoWindow);
    }
}

void Example::ExampleUi::DrawFloatingWindow()
{
    if (!m_ShowFloatingWindow)
    {
        m_FloatingWindowPlaced = false;
        return;
    }

    if (!m_FloatingWindowPlaced)
    {
        const ImGuiViewport* const Main = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(Main->Pos.x + Main->Size.x + 48.F, Main->Pos.y + 90.F), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(320.F, 200.F), ImGuiCond_Always);
        m_FloatingWindowPlaced = true;
    }

    if (ImGui::Begin("Viewport Demo", &m_ShowFloatingWindow))
    {
        ImGui::TextColored(ImVec4(0.35F, 0.90F, 0.90F, 1.F), "SECOND OS WINDOW");
        ImGui::TextWrapped("This window lives in its own Dear ImGui viewport: a luvk::PresentTarget presented in the same "
                           "submission as the main window.");
        ImGui::Separator();
        ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
    }
    ImGui::End();
}

void Example::ExampleUi::Focus(const glm::vec3& Target) const
{
    luvk::app::Camera& Camera = m_App.GetCamera();
    Camera.Position           = Target + glm::vec3{0.F, 1.1F, 3.6F};
    Camera.Yaw                = -90.F;
    Camera.Pitch              = -14.F;
}

void Example::ExampleUi::ImageFit(const ImTextureID Texture, const float MaxSide)
{
    const float Side = std::max(1.F, std::min(ImGui::GetContentRegionAvail().x, MaxSide));
    ImGui::Image(ImTextureRef(Texture), ImVec2(Side, Side));
}
