// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/MeshFeature.hpp"
#include <imgui.h>
#include <luvk/Frame/Slots.hpp>

void luvk::app::MeshFeature::OnAttach(Context& Ctx)
{
    m_Renderer = std::make_unique<scene::MeshRenderer>(Ctx);
}

void luvk::app::MeshFeature::OnDetach(Context&)
{
    m_Renderer.reset();
}

void luvk::app::MeshFeature::AddPasses(frame::FeatureContext& Frame)
{
    const frame::ViewData* const View = Frame.Board.Find(frame::Slots::View);
    m_Renderer->AddPasses(Frame.Graph,
                          Frame.Allocator,
                          scene::MeshTargets{.Color          = View != nullptr ? Frame.Board.FindTexture(frame::Slots::SceneColor) : TextureHandle{},
                                             .Depth          = Frame.Board.FindTexture(frame::Slots::SceneDepth),
                                             .ColorFormat    = Frame.Settings.ColorFormat,
                                             .DepthFormat    = Frame.Settings.DepthFormat,
                                             .ViewProjection = View != nullptr ? View->ViewProjection : glm::mat4{1.F},
                                             .CameraPosition = View != nullptr ? View->Position : glm::vec3{0.F},
                                             .Wireframe      = Frame.Settings.Wireframe,
                                             .ReverseDepth   = Frame.Settings.ReverseDepth});
}

void luvk::app::MeshFeature::OnGui()
{
    ImGui::Text("Draw calls: %u   Instances: %u", m_Renderer->LastDrawCount(), m_Renderer->LastInstanceCount());
    ImGui::SliderFloat3("Light direction", &m_Renderer->Light.Direction.x, -1.F, 1.F);
    ImGui::ColorEdit3("Light color", &m_Renderer->Light.Color.x, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
    ImGui::SliderFloat("Ambient", &m_Renderer->Light.Ambient, 0.F, 1.F);
}

void luvk::app::MeshFeature::Draw(const scene::Mesh& Target, const scene::Material& Surface, const glm::mat4& Transform)
{
    m_Renderer->Draw(Target, Surface, Transform);
}

void luvk::app::MeshFeature::DrawInstanced(const scene::Mesh& Target, const scene::Material& Surface, const std::span<const glm::mat4> Transforms)
{
    m_Renderer->DrawInstanced(Target, Surface, Transforms);
}
