// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include <type_traits>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <luvk/App/Camera.hpp>
#include <luvk/Core/Context.hpp>
#include <luvk/Core/Module.hpp>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Shaders/ShaderCompiler.hpp>
#include "Shaders/MeshShaderCube.hpp"
#include "MeshShaderCube.hpp"

namespace
{
    using Shaders::MeshShaderCube::Shader;
}

void Example::MeshShaderCube::OnAttach(luvk::Context& Ctx)
{
    m_Available = luvk::MeshShaderModule::IsAvailable(Ctx);
    m_Status    = m_Available ? "ready: 1 task group -> 6 mesh groups (4 vertices, 2 triangles each)"
                                             : "unavailable: VK_EXT_mesh_shader is not supported by this device";
    SetEnabled(m_Available);
}

void Example::MeshShaderCube::Update(const float DeltaTime)
{
    m_Angle += DeltaTime * m_Speed;
}

glm::mat4 Example::MeshShaderCube::Transform() const
{
    const glm::mat4 Placed = glm::translate(glm::mat4{1.F}, m_Position);
    return glm::rotate(glm::rotate(Placed, m_Angle, glm::vec3{0.F, 1.F, 0.F}), m_Angle * 0.5F, glm::vec3{1.F, 0.F, 0.F});
}

void Example::MeshShaderCube::AddPasses(luvk::frame::FeatureContext& Frame)
{
    static_assert(std::is_trivially_copyable_v<glm::mat4>);
    static_assert(sizeof(glm::mat4) <= luvk::PushConstantSize);
    if (!m_Available)
    {
        return;
    }

    if (m_Format != Frame.Settings.ColorFormat || m_Wireframe != Frame.Settings.Wireframe || m_ReverseDepth != Frame.Settings.ReverseDepth)
    {
        const auto Task     = luvk::CompileShaderOrThrow(Shader, "mesh_cube", "TaskMain");
        const auto Mesh     = luvk::CompileShaderOrThrow(Shader, "mesh_cube", "MeshMain");
        const auto Fragment = luvk::CompileShaderOrThrow(Shader, "mesh_cube", "FragmentMain");

        luvk::GraphicsPipelineDesc Desc{.Task         = {.Code = Task},
                                        .Mesh         = {.Code = Mesh},
                                        .Fragment     = {.Code = Fragment},
                                        .ColorFormats = {Frame.Settings.ColorFormat},
                                        .DepthFormat  = Frame.Settings.DepthFormat,
                                        .PolygonMode  = Frame.Settings.PolygonMode(),
                                        .CullMode     = luvk::CullMode::None,
                                        .DepthCompare = Frame.Settings.DepthCompare(),
                                        .Name         = "mesh shader cube"};
        m_Pipeline     = luvk::Pipeline(Frame.Ctx, Desc);
        m_Format       = Frame.Settings.ColorFormat;
        m_Wireframe    = Frame.Settings.Wireframe;
        m_ReverseDepth = Frame.Settings.ReverseDepth;
    }

    const auto* const View = Frame.Board.Find(luvk::frame::Slots::View);
    if (View == nullptr)
    {
        return;
    }

    const glm::mat4             MVP      = View->ViewProjection * Transform();
    const luvk::Pipeline* const Selected = &m_Pipeline;

    const luvk::TextureHandle Color = Frame.Board.FindTexture(luvk::frame::Slots::SceneColor);
    const luvk::TextureHandle Depth = Frame.Board.FindTexture(luvk::frame::Slots::SceneDepth);

    Frame.Graph.AddPass("Mesh Shader Cube",
                        luvk::PassType::Graphics,
                        [&](luvk::PassBuilder& Pass)
                        {
                            Pass.ColorAttachment(Color, luvk::LoadOp::Load);
                            Pass.DepthAttachment(Depth, luvk::LoadOp::Load);
                        },
                        [Selected, MVP](luvk::CommandList& Cmd)
                        {
                            Cmd.BindPipeline(*Selected);
                            Cmd.PushConstants(MVP);
                            Cmd.DrawMeshTasks(1U);
                        });
}

void Example::MeshShaderCube::OnGui()
{
    ImGui::TextWrapped("%s", m_Status.c_str());
    ImGui::SliderFloat("Rotation speed", &m_Speed, -3.F, 3.F);
}
