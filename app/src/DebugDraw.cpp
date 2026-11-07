// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/DebugDraw.hpp"
#include <array>
#include <cmath>
#include <span>
#include <stdexcept>
#include <type_traits>
#include <imgui.h>
#include <glm/gtc/constants.hpp>
#include <luvk/Core/Context.hpp>
#include <luvk/Render/CommandList.hpp>
#include "Shaders/EmbeddedShaders.hpp"
#include "luvk/App/Camera.hpp"

namespace
{
    std::uint32_t Pack(const glm::vec4& Color)
    {
        const glm::uvec4 Bytes = glm::uvec4(glm::clamp(Color, 0.F, 1.F) * 255.F + 0.5F);
        return Bytes.r | (Bytes.g << 8U) | (Bytes.b << 16U) | (Bytes.a << 24U);
    }

    struct DebugDrawPushConstants
    {
        glm::mat4 ViewProjection{1.F};
    };

    static_assert(std::is_trivially_copyable_v<DebugDrawPushConstants>);
    static_assert(sizeof(DebugDrawPushConstants) <= luvk::PushConstantSize);

    /** Push constants of DebugDrawGpu.vert. */
    struct DebugDrawGpuPushConstants
    {
        glm::mat4       ViewProjection{1.F};
        VkDeviceAddress Lines = 0U;
    };

    static_assert(std::is_trivially_copyable_v<DebugDrawGpuPushConstants>);
    static_assert(sizeof(DebugDrawGpuPushConstants) <= luvk::PushConstantSize);

    constexpr std::array<std::array<std::uint8_t, 2>, 12> BoxEdges{{{0, 1}, {2, 3}, {4, 5}, {6, 7}, {0, 2}, {1, 3}, {4, 6}, {5, 7}, {0, 4}, {1, 5}, {2, 6}, {3, 7}}};
}

void luvk::app::DebugDraw::CreatePipelines(Context& Ctx, const VkFormat ColorFormat, const VkFormat DepthFormat, const VkCompareOp DepthCompare)
{
    GraphicsPipelineDesc Desc{.Vertex       = {.Code = shaders::DebugDrawVertexShader()},
                              .Fragment     = {.Code = shaders::DebugDrawFragmentShader()},
                              .ColorFormats = {ColorFormat},
                              .Blend        = {BlendMode::Alpha},
                              .DepthFormat  = DepthFormat,
                              .VertexBindings = {{.binding   = 0U,
                                                  .stride    = static_cast<std::uint32_t>(sizeof(Vertex)),
                                                  .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
                              .VertexAttributes = {{.location = 0U,
                                                    .binding  = 0U,
                                                    .format   = VK_FORMAT_R32G32B32_SFLOAT,
                                                    .offset   = static_cast<std::uint32_t>(offsetof(Vertex, Position))},
                                                   {.location = 1U,
                                                    .binding  = 0U,
                                                    .format   = VK_FORMAT_R8G8B8A8_UNORM,
                                                    .offset   = static_cast<std::uint32_t>(offsetof(Vertex, Color))}},
                              .Topology     = VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                              .CullMode     = VK_CULL_MODE_NONE,
                              .DepthTest    = true,
                              .DepthWrite   = false,
                              .DepthCompare = DepthCompare,
                              .Name         = "debug draw (depth tested)"};
    m_Tested = Pipeline(Ctx, Desc);

    Desc.DepthTest = false;
    Desc.Name      = "debug draw (overlay)";
    m_Overlay      = Pipeline(Ctx, Desc);

    // GPU lines: same raster state, vertices pulled from the GPU-written buffer.
    Desc.Vertex           = {.Code = shaders::DebugDrawGpuVertexShader()};
    Desc.VertexBindings   = {};
    Desc.VertexAttributes = {};
    Desc.DepthTest        = true;
    Desc.Name             = "debug draw GPU lines (depth tested)";
    m_GpuTested           = Pipeline(Ctx, Desc);

    Desc.DepthTest = false;
    Desc.Name      = "debug draw GPU lines (overlay)";
    m_GpuOverlay   = Pipeline(Ctx, Desc);

    m_ColorFormat = ColorFormat;
    m_DepthFormat  = DepthFormat;
    m_DepthCompare = DepthCompare;
}

void luvk::app::DebugDraw::AddPasses(FeatureContext& Frame)
{
    m_LastVertexCount = static_cast<std::uint32_t>(std::size(m_Vertices));

    const ViewData* const View     = Frame.Board.Find<ViewData>(Slots::View);
    const BufferHandle    GpuLines = Frame.Board.FindBuffer(Slots::DebugLines);
    m_LastGpuLines                 = GpuLines.IsValid() && View != nullptr;

    if (View == nullptr || (std::empty(m_Vertices) && !GpuLines.IsValid()))
    {
        m_Vertices.clear();
        return;
    }

    if (m_ColorFormat != Frame.Settings.ColorFormat || m_DepthFormat != Frame.Settings.DepthFormat || m_DepthCompare != Frame.Settings.DepthCompare())
    {
        CreatePipelines(Frame.Ctx, Frame.Settings.ColorFormat, Frame.Settings.DepthFormat, Frame.Settings.DepthCompare());
    }

    const TextureHandle Color = Frame.Board.FindTexture(Slots::SceneColor);
    const TextureHandle Depth = Frame.Board.FindTexture(Slots::SceneDepth);

    if (!std::empty(m_Vertices))
    {
        const FrameAllocation        Vertices = Frame.Allocator.Upload(std::span<const Vertex>(m_Vertices));
        const DebugDrawPushConstants Push{.ViewProjection = View->ViewProjection};
        const std::uint32_t          Count    = m_LastVertexCount;
        const Pipeline* const        Selected = DepthTest ? &m_Tested : &m_Overlay;

        Frame.Graph.AddPass("Debug Draw",
                            PassType::Graphics,
                            [&](PassBuilder& Pass)
                            {
                                Pass.ColorAttachment(Color, LoadOp::Load);
                                Pass.DepthAttachment(Depth, LoadOp::Load, Frame.Settings.ClearDepth(), false);
                            },
                            [Push, Count, Selected, Vertices](CommandList& Cmd)
                            {
                                Cmd.BindPipeline(*Selected);
                                Cmd.BindVertexBuffer(0U, Vertices);
                                Cmd.PushConstants(Push);
                                Cmd.Draw(Count);
                            });
    }

    if (GpuLines.IsValid())
    {
        const glm::mat4       ViewProjection = View->ViewProjection;
        const Pipeline* const Selected       = DepthTest ? &m_GpuTested : &m_GpuOverlay;

        Frame.Graph.AddPass("Debug Draw (GPU lines)",
                            PassType::Graphics,
                            [&](PassBuilder& Pass)
                            {
                                Pass.Use(GpuLines, Access::IndirectBuffer);
                                Pass.Use(GpuLines, Access::ShaderStorageRead);
                                Pass.ColorAttachment(Color, LoadOp::Load);
                                Pass.DepthAttachment(Depth, LoadOp::Load, Frame.Settings.ClearDepth(), false);
                            },
                            [ViewProjection, Selected, GpuLines](CommandList& Cmd)
                            {
                                const DebugDrawGpuPushConstants Push{.ViewProjection = ViewProjection, .Lines = Cmd.ResolveBuffer(GpuLines).Address};
                                Cmd.BindPipeline(*Selected);
                                Cmd.PushConstants(Push);
                                Cmd.DrawIndirect(GpuLines, 1U);
                            });
    }

    m_Vertices.clear();
}

luvk::BufferHandle luvk::app::DebugDraw::AcquireGpuLines(FeatureContext& Frame, const std::uint32_t MaxLines)
{
    if (const BufferHandle Existing = Frame.Board.FindBuffer(Slots::DebugLines); Existing.IsValid())
    {
        return Existing;
    }

    if (MaxLines == 0U)
    {
        throw std::invalid_argument("[luvk] DebugDraw::AcquireGpuLines needs room for at least one line.");
    }

    const std::uint32_t       Capacity = MaxLines * 2U;
    const GpuDebugLinesHeader Header{.Capacity = Capacity};

    const BufferHandle Lines = Frame.Graph.CreateBuffer({.Size = sizeof(GpuDebugLinesHeader) + static_cast<VkDeviceSize>(Capacity) * sizeof(GpuDebugLineVertex),
                                                         .Name = "DebugLines"});

    // Only the header is reset: vertices past VertexCount are never read.
    const FrameAllocation Reset = Frame.Allocator.Upload(std::span<const GpuDebugLinesHeader>(&Header, 1U));
    Frame.Graph.AddPass("Debug Lines Reset",
                        PassType::Transfer,
                        [&](PassBuilder& Pass) { Pass.Use(Lines, Access::TransferDst); },
                        [Reset, Lines](CommandList& Cmd) { Cmd.CopyBuffer(Reset, Lines); });

    Frame.Board.SetBuffer(Slots::DebugLines, Lines);
    return Lines;
}

void luvk::app::DebugDraw::OnGui()
{
    ImGui::Checkbox("Depth test", &DepthTest);
    ImGui::Text("Vertices last frame: %u", m_LastVertexCount);
    ImGui::Text("GPU lines last frame: %s", m_LastGpuLines ? "yes" : "no");
}

void luvk::app::DebugDraw::Line(const glm::vec3& From, const glm::vec3& To, const glm::vec4& Color)
{
    const std::uint32_t Packed = Pack(Color);
    m_Vertices.push_back({From, Packed});
    m_Vertices.push_back({To, Packed});
}

void luvk::app::DebugDraw::Box(const glm::vec3& Min, const glm::vec3& Max, const glm::vec4& Color, const glm::mat4& Transform)
{
    std::array<glm::vec3, 8> Corners;
    for (std::size_t Index = 0U; Index < 8U; ++Index)
    {
        const glm::vec3 Local((Index & 1U) != 0U ? Max.x : Min.x, (Index & 2U) != 0U ? Max.y : Min.y, (Index & 4U) != 0U ? Max.z : Min.z);
        Corners[Index] = glm::vec3(Transform * glm::vec4(Local, 1.F));
    }

    for (const auto& [A, B] : BoxEdges)
    {
        Line(Corners[A], Corners[B], Color);
    }
}

void luvk::app::DebugDraw::Frustum(const glm::mat4& InverseViewProjection, const glm::vec4& Color)
{
    std::array<glm::vec3, 8> Corners;
    for (std::size_t Index = 0U; Index < 8U; ++Index)
    {
        const glm::vec4 Clip((Index & 1U) != 0U ? 1.F : -1.F, (Index & 2U) != 0U ? 1.F : -1.F, (Index & 4U) != 0U ? 1.F : 0.F, 1.F);
        const glm::vec4 World = InverseViewProjection * Clip;
        Corners[Index]        = glm::vec3(World) / World.w;
    }

    for (const auto& [A, B] : BoxEdges)
    {
        Line(Corners[A], Corners[B], Color);
    }
}

void luvk::app::DebugDraw::Grid(const float HalfSize, const int Divisions, const glm::vec4& Color)
{
    const float Step = 2.F * HalfSize / static_cast<float>(Divisions);
    for (int Index = 0; Index <= Divisions; ++Index)
    {
        const float Offset = -HalfSize + Step * static_cast<float>(Index);
        Line({Offset, 0.F, -HalfSize}, {Offset, 0.F, HalfSize}, Color);
        Line({-HalfSize, 0.F, Offset}, {HalfSize, 0.F, Offset}, Color);
    }
}

void luvk::app::DebugDraw::Axes(const glm::mat4& Transform, const float Size)
{
    const glm::vec3 Origin = glm::vec3(Transform[3]);
    Line(Origin, Origin + glm::vec3(Transform[0]) * Size, {1.F, 0.2F, 0.2F, 1.F});
    Line(Origin, Origin + glm::vec3(Transform[1]) * Size, {0.2F, 1.F, 0.2F, 1.F});
    Line(Origin, Origin + glm::vec3(Transform[2]) * Size, {0.3F, 0.5F, 1.F, 1.F});
}

void luvk::app::DebugDraw::Circle(const glm::vec3& Center, const glm::vec3& Normal, const float Radius, const glm::vec4& Color, const int Segments)
{
    const glm::vec3 N        = glm::normalize(Normal);
    const glm::vec3 Helper   = std::abs(N.y) < 0.99F ? glm::vec3{0.F, 1.F, 0.F} : glm::vec3{1.F, 0.F, 0.F};
    const glm::vec3 TangentA = glm::normalize(glm::cross(N, Helper));
    const glm::vec3 TangentB = glm::cross(N, TangentA);

    glm::vec3 Previous = Center + TangentA * Radius;
    for (int Index = 1; Index <= Segments; ++Index)
    {
        const float     Angle = glm::two_pi<float>() * static_cast<float>(Index) / static_cast<float>(Segments);
        const glm::vec3 Point = Center + (TangentA * std::cos(Angle) + TangentB * std::sin(Angle)) * Radius;
        Line(Previous, Point, Color);
        Previous = Point;
    }
}
