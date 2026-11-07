// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "Particles.hpp"
#include <algorithm>
#include <random>
#include <type_traits>
#include <vector>
#include <imgui.h>
#include <luvk/App/DebugDraw.hpp>
#include <luvk/App/Scene.hpp>
#include <luvk/Core/Context.hpp>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Shaders/ShaderCompiler.hpp>
#include "Shaders/Particles.hpp"

namespace
{
    using Shaders::Particles::Shader;

    constexpr std::uint32_t ParticleCount = 8192U;

    static_assert(sizeof(Example::Particles::Particle) == 32U, "Particle must match the shader layout.");
}

void Example::Particles::OnAttach(luvk::Context& Ctx)
{
    // Every particle starts "dead" with a random delay, so the fountain fills up over its first seconds.
    std::mt19937                          Generator{7U};
    std::uniform_real_distribution<float> Distribution(0.F, 1.F);

    std::vector<Particle> Initial(ParticleCount);
    for (Particle& Item : Initial)
    {
        Item = Particle{.Position = Emitter, .Life = -2.5F * Distribution(Generator), .Velocity = glm::vec3{0.F}, .Seed = Distribution(Generator)};
    }

    m_Buffer = luvk::Buffer(Ctx, {.Size = ParticleCount * sizeof(Particle), .Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .Name = "particles"});
    m_Buffer.Write(std::span<const Particle>(Initial));

    m_Compute = luvk::Pipeline(Ctx,
                               luvk::ComputePipelineDesc{.Compute = {.Code = luvk::CompileShaderOrThrow(Shader, "particles", "ComputeMain")},
                                                         .Name    = "particles (simulate)"});
}

void Example::Particles::Update(const float DeltaTime)
{
    m_DeltaTime = DeltaTime * m_Speed;
    m_Time += m_DeltaTime;
}

void Example::Particles::AddPasses(luvk::app::FeatureContext& Frame)
{
    static_assert(std::is_trivially_copyable_v<PushData>);
    static_assert(sizeof(PushData) <= luvk::PushConstantSize);

    const auto* const View = Frame.Board.Find<luvk::app::ViewData>(luvk::app::Slots::View);
    if (View == nullptr)
    {
        return;
    }

    if (m_Format != Frame.Settings.ColorFormat || m_Wireframe != Frame.Settings.Wireframe || m_ReverseDepth != Frame.Settings.ReverseDepth)
    {
        m_Graphics  = luvk::Pipeline(Frame.Ctx,
                                     luvk::GraphicsPipelineDesc{.Vertex       = {.Code = luvk::CompileShaderOrThrow(Shader, "particles", "VertexMain")},
                                                                .Fragment     = {.Code = luvk::CompileShaderOrThrow(Shader, "particles", "FragmentMain")},
                                                                .ColorFormats = {Frame.Settings.ColorFormat},
                                                                .Blend        = {luvk::BlendMode::Alpha},
                                                                .DepthFormat  = Frame.Settings.DepthFormat,
                                                                .PolygonMode  = Frame.Settings.PolygonMode(),
                                                                .CullMode     = VK_CULL_MODE_NONE,
                                                                .DepthTest    = true,
                                                                .DepthWrite   = false,
                                                                .DepthCompare = Frame.Settings.DepthCompare(),
                                                                .Name         = "particles (draw)"});
        m_Format       = Frame.Settings.ColorFormat;
        m_Wireframe    = Frame.Settings.Wireframe;
        m_ReverseDepth = Frame.Settings.ReverseDepth;
    }

    // The camera basis comes from the rows of the view matrix (world -> view).
    const PushData Push{.ViewProjection = View->ViewProjection,
                        .Particles      = m_Buffer.Address(),
                        .Count          = ParticleCount,
                        .DeltaTime      = m_DeltaTime,
                        .Emitter        = Emitter,
                        .Time           = m_Time,
                        .CameraRight    = {View->View[0][0], View->View[1][0], View->View[2][0]},
                        .Size           = m_Size,
                        .CameraUp       = {View->View[0][1], View->View[1][1], View->View[2][1]}};

    const luvk::BufferHandle  Data  = Frame.Graph.Import(m_Buffer);
    const luvk::TextureHandle Color = Frame.Board.FindTexture(luvk::app::Slots::SceneColor);
    const luvk::TextureHandle Depth = Frame.Board.FindTexture(luvk::app::Slots::SceneDepth);

    Frame.Graph.AddPass("Particles: simulate",
                        luvk::PassType::Compute,
                        [&](luvk::PassBuilder& Pass)
                        {
                            Pass.Use(Data, luvk::Access::ShaderStorageWrite);
                        },
                        [this, Push](luvk::CommandList& Cmd)
                        {
                            Cmd.BindPipeline(m_Compute);
                            Cmd.PushConstants(Push);
                            Cmd.Dispatch((ParticleCount + 63U) / 64U);
                        });

    if (m_ShowVelocities)
    {
        AddVelocityLines(Frame, Data);
    }

    Frame.Graph.AddPass("Particles: draw",
                        luvk::PassType::Graphics,
                        [&](luvk::PassBuilder& Pass)
                        {
                            Pass.Use(Data, luvk::Access::ShaderStorageRead);
                            Pass.ColorAttachment(Color, luvk::LoadOp::Load);
                            Pass.DepthAttachment(Depth, luvk::LoadOp::Load, Frame.Settings.ClearDepth(), false);
                        },
                        [this, Push](luvk::CommandList& Cmd)
                        {
                            Cmd.BindPipeline(m_Graphics);
                            Cmd.PushConstants(Push);
                            Cmd.Draw(6U, ParticleCount);
                        });
}

void Example::Particles::AddVelocityLines(luvk::app::FeatureContext& Frame, const luvk::BufferHandle Data)
{
    const auto Stride = static_cast<std::uint32_t>(std::max(1, m_DebugStride));
    if (Stride != m_DebugBuiltStride)
    {
        // Same SPIR-V, a different specialization: the Slang source is compiled once, whatever the stride.
        static const std::vector<std::uint32_t> Code = luvk::CompileShaderOrThrow(Shader, "particles", "DebugMain");
        m_Debug = luvk::Pipeline(Frame.Ctx,
                                 luvk::ComputePipelineDesc{.Compute = {.Code = Code, .Constants = {luvk::SpecializationConstant::UInt(0U, Stride)}},
                                                           .Name    = "particles (velocity lines)"});
        m_DebugBuiltStride = Stride;
    }

    const luvk::BufferHandle Lines = luvk::app::DebugDraw::AcquireGpuLines(Frame);
    const float              Scale = m_VelocityScale;

    Frame.Graph.AddPass("Particles: velocity lines",
                        luvk::PassType::Compute,
                        [&](luvk::PassBuilder& Pass)
                        {
                            Pass.Use(Data, luvk::Access::ShaderStorageRead);
                            Pass.Use(Lines, luvk::Access::ShaderStorageWrite);
                        },
                        [this, Data, Lines, Scale, Stride](luvk::CommandList& Cmd)
                        {
                            const VkDeviceAddress Header = Cmd.ResolveBuffer(Lines).Address;
                            const DebugPush       Push{.Particles = Cmd.ResolveBuffer(Data).Address,
                                                       .Lines     = Header,
                                                       .Vertices  = Header + sizeof(luvk::app::GpuDebugLinesHeader),
                                                       .Count     = ParticleCount,
                                                       .Scale     = Scale};
                            Cmd.BindPipeline(m_Debug);
                            Cmd.PushConstants(Push);
                            Cmd.Dispatch((ParticleCount / Stride + 63U) / 64U);
                        });
}

void Example::Particles::OnGui()
{
    ImGui::Text("%u particles, simulated and drawn on the GPU", ParticleCount);
    ImGui::SliderFloat("Particle size", &m_Size, 0.01F, 0.1F);
    ImGui::SliderFloat("Simulation speed", &m_Speed, 0.F, 2.F);
    ImGui::Checkbox("Velocity lines (GPU debug draw)", &m_ShowVelocities);
    if (m_ShowVelocities)
    {
        ImGui::SliderInt("Line every N particles", &m_DebugStride, 1, 64);
        ImGui::SliderFloat("Line length (s)", &m_VelocityScale, 0.02F, 0.5F);
    }
}
