// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "ImGuiRenderer.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <type_traits>
#include <luvk/Core/Bindless.hpp>
#include <luvk/Core/Context.hpp>
#include <luvk/Core/Error.hpp>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Resources/Image.hpp>
#include "ImGuiShaders.hpp"
#include "ImGuiViewportWindow.hpp"
#include "ImGuiRendererInternals.hpp"
luvk::imgui::ImGuiRenderer::ImGuiRenderer(Context& Ctx, const std::uint32_t FramesInFlight, const bool EnableViewports)
    : m_Context(Ctx),
      m_FrameCount(std::max(1U, FramesInFlight)),
      m_Viewports(EnableViewports),
      m_Sampler(Ctx,
                {.AddressU = AddressMode::ClampToEdge,
                 .AddressV = AddressMode::ClampToEdge,
                 .AddressW = AddressMode::ClampToEdge,
                 .Name     = "luvk ImGui sampler"})
{
    m_WhiteTexture = std::make_unique<Image>(m_Context,
                                             ImageDesc{.Extent = {1U, 1U, 1U},
                                                       .Format = Format::R8G8B8A8Unorm,
                                                       .Usage  = ImageUsage::Sampled,
                                                       .Name   = "luvk ImGui white"});

    constexpr std::array<std::uint8_t, 4> White{255U, 255U, 255U, 255U};
    m_WhiteTexture->Upload(std::as_bytes(std::span(White)));
    m_WhiteIndex = m_WhiteTexture->SampledIndex();

    ImGuiIO& IO = ImGui::GetIO();
    IO.BackendRendererName     = "luvk (bindless)";
    IO.BackendRendererUserData = this;
    IO.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
    if (m_Viewports)
    {
        IO.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;
    }

    // Secondary viewports are recorded into the main frame by ImGuiLayer::AddViewportPasses: no render/swap callbacks.
    ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
    PlatformIO.Renderer_CreateWindow  = detail::ImGuiCreateViewportWindow;
    PlatformIO.Renderer_DestroyWindow = detail::ImGuiDestroyViewportWindow;

    RefreshLiveImages();
}

luvk::imgui::ImGuiRenderer::~ImGuiRenderer()
{
    m_Pipelines.clear();
    m_BackendTextures.clear();
    m_UserTextures.clear();
    m_LiveImages.clear();
    m_ValidIndices.clear();
    m_WhiteTexture.reset();

    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiPlatformIO& PlatformIO = ImGui::GetPlatformIO();
        PlatformIO.Renderer_CreateWindow  = nullptr;
        PlatformIO.Renderer_DestroyWindow = nullptr;

        ImGuiIO& IO = ImGui::GetIO();
        if (IO.BackendRendererUserData == this)
        {
            IO.BackendRendererUserData = nullptr;
            IO.BackendRendererName     = nullptr;
        }
    }
}

luvk::Pipeline& luvk::imgui::ImGuiRenderer::PipelineForFormat(const luvk::Format Format)
{
    if (const auto It = m_Pipelines.find(Format); It != std::end(m_Pipelines))
    {
        return *It->second;
    }

    const GraphicsPipelineDesc Desc{.Vertex       = {.Code = Shaders::ImGuiVertex()},
                                    .Fragment     = {.Code = Shaders::ImGuiFragment()},
                                    .ColorFormats = {Format},
                                    .Blend        = {BlendMode::Alpha},
                                    .CullMode     = CullMode::None,
                                    .DepthTest    = false,
                                    .DepthWrite   = false,
                                    .PushSize     = sizeof(PushConstants),
                                    .Name         = "luvk ImGui"};

    auto            Created = std::make_unique<luvk::Pipeline>(m_Context, Desc);
    luvk::Pipeline& Result  = *Created;
    m_Pipelines.emplace(Format, std::move(Created));
    return Result;
}

luvk::imgui::ImGuiGeometry luvk::imgui::ImGuiRenderer::UploadDrawData(const ImDrawData& DrawData, FrameAllocator& Allocator) const
{
    if (DrawData.TotalVtxCount <= 0 || DrawData.TotalIdxCount <= 0)
    {
        return {};
    }

    // One allocation per stream for the whole frame: every draw list is a plain memcpy into mapped memory.
    const ImGuiGeometry Geometry{.Vertices = Allocator.Allocate(static_cast<DeviceSize>(DrawData.TotalVtxCount) * sizeof(ImDrawVert)),
                                 .Indices  = Allocator.Allocate(static_cast<DeviceSize>(DrawData.TotalIdxCount) * sizeof(ImDrawIdx))};

    DeviceSize VertexOffset = 0U;
    DeviceSize IndexOffset  = 0U;
    for (const ImDrawList* const List : DrawData.CmdLists)
    {
        const std::span<const ImDrawVert> Vertices(List->VtxBuffer.Data, static_cast<std::size_t>(List->VtxBuffer.Size));
        const std::span<const ImDrawIdx>  Indices(List->IdxBuffer.Data, static_cast<std::size_t>(List->IdxBuffer.Size));

        Geometry.Vertices.Write(Vertices, VertexOffset);
        Geometry.Indices.Write(Indices, IndexOffset);
        VertexOffset += Vertices.size_bytes();
        IndexOffset += Indices.size_bytes();
    }

    return Geometry;
}

void luvk::imgui::ImGuiRenderer::RenderDrawData(CommandList& Cmd,
                                              const ImDrawData& DrawData,
                                              const ImGuiGeometry& Geometry,
                                              const Extent2D TargetExtent,
                                              const luvk::Format Format)
{
    static_assert(std::is_trivially_copyable_v<PushConstants>);
    static_assert(sizeof(PushConstants) <= PushConstantSize);

    static_assert(sizeof(ImDrawVert) == 20U);
    static_assert(offsetof(ImDrawVert, pos) == 0U);
    static_assert(offsetof(ImDrawVert, uv) == 8U);
    static_assert(offsetof(ImDrawVert, col) == 16U);

    const int FramebufferWidth  = static_cast<int>(DrawData.DisplaySize.x * DrawData.FramebufferScale.x + 0.5F);
    const int FramebufferHeight = static_cast<int>(DrawData.DisplaySize.y * DrawData.FramebufferScale.y + 0.5F);
    if (FramebufferWidth <= 0 || FramebufferHeight <= 0 || !Geometry.Vertices || !Geometry.Indices)
    {
        return;
    }

    Cmd.SetViewport(TargetExtent);
    Cmd.BindPipeline(PipelineForFormat(Format));
    Cmd.BindIndexBuffer(Geometry.Indices, sizeof(ImDrawIdx) == 2U ? IndexType::UInt16 : IndexType::UInt32);

    const ImVec2 Scale{2.F / DrawData.DisplaySize.x, 2.F / DrawData.DisplaySize.y};
    const ImVec2 Translate{-1.F - DrawData.DisplayPos.x * Scale.x, -1.F - DrawData.DisplayPos.y * Scale.y};
    const ImVec2 ClipOffset = DrawData.DisplayPos;
    const ImVec2 ClipScale  = DrawData.FramebufferScale;

    std::uint32_t GlobalVertexOffset = 0U;
    std::uint32_t GlobalIndexOffset  = 0U;

    for (const ImDrawList* const List : DrawData.CmdLists)
    {
        for (const ImDrawCmd& Command : List->CmdBuffer)
        {
            if (Command.UserCallback != nullptr)
            {
                // The backend owns no state beyond the pipeline/buffers bound for this draw data, so a reset is a no-op.
                // Never call the sentinel value itself: it is not a real callback address.
                const ImDrawCallback ResetState = ImGui::GetPlatformIO().DrawCallback_ResetRenderState;
                const bool          IsReset    = (ResetState != nullptr && Command.UserCallback == ResetState) ||
                                       reinterpret_cast<std::intptr_t>(Command.UserCallback) == static_cast<std::intptr_t>(-8);

                if (!IsReset)
                {
                    Command.UserCallback(List, &Command);
                }
            }
            else
            {
                ImVec2 ClipMin((Command.ClipRect.x - ClipOffset.x) * ClipScale.x, (Command.ClipRect.y - ClipOffset.y) * ClipScale.y);
                ImVec2 ClipMax((Command.ClipRect.z - ClipOffset.x) * ClipScale.x, (Command.ClipRect.w - ClipOffset.y) * ClipScale.y);

                ClipMin.x = std::clamp(ClipMin.x, 0.F, static_cast<float>(FramebufferWidth));
                ClipMin.y = std::clamp(ClipMin.y, 0.F, static_cast<float>(FramebufferHeight));
                ClipMax.x = std::clamp(ClipMax.x, 0.F, static_cast<float>(FramebufferWidth));
                ClipMax.y = std::clamp(ClipMax.y, 0.F, static_cast<float>(FramebufferHeight));
                if (ClipMax.x <= ClipMin.x || ClipMax.y <= ClipMin.y)
                {
                    continue;
                }

                Cmd.SetScissor({.Offset = {static_cast<std::int32_t>(ClipMin.x), static_cast<std::int32_t>(ClipMin.y)},
                                .Extent = {static_cast<std::uint32_t>(ClipMax.x - ClipMin.x), static_cast<std::uint32_t>(ClipMax.y - ClipMin.y)}});

                const std::uint32_t Sampled = detail::ToSampledIndex(Command.GetTexID());
                const std::uint32_t Texture = m_ValidIndices.contains(Sampled) ? Sampled : m_WhiteIndex;

                const PushConstants Push{.Scale     = {Scale.x, Scale.y},
                                         .Translate = {Translate.x, Translate.y},
                                         .Vertices  = Geometry.Vertices.Address,
                                         .Texture   = {Texture},
                                         .Sampler   = m_Sampler.Bindless()};
                Cmd.PushConstants(Push);
                Cmd.DrawIndexed(Command.ElemCount,
                                1U,
                                Command.IdxOffset + GlobalIndexOffset,
                                static_cast<std::int32_t>(Command.VtxOffset + GlobalVertexOffset),
                                0U);
            }
        }

        GlobalVertexOffset += static_cast<std::uint32_t>(List->VtxBuffer.Size);
        GlobalIndexOffset += static_cast<std::uint32_t>(List->IdxBuffer.Size);
    }

    Cmd.SetScissor({.Offset = {0, 0}, .Extent = TargetExtent});
}



