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
#include "ImGuiViewportWindow.hpp"
#include "Shaders/ImGuiShaders.hpp"
#include "ImGuiRendererInternals.hpp"
luvk::imgui::ImGuiRenderer::ImGuiRenderer(Context& Ctx, const std::uint32_t FramesInFlight, const bool EnableViewports)
    : m_Context(Ctx),
      m_FrameCount(std::max(1U, FramesInFlight)),
      m_Viewports(EnableViewports),
      m_Sampler(Ctx,
                {.AddressU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                 .AddressV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                 .AddressW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                 .Name     = "luvk ImGui sampler"})
{
    m_WhiteTexture = std::make_unique<Image>(m_Context,
                                             ImageDesc{.Extent = {1U, 1U, 1U},
                                                       .Format = VK_FORMAT_R8G8B8A8_UNORM,
                                                       .Usage  = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
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

luvk::Pipeline& luvk::imgui::ImGuiRenderer::PipelineForFormat(const VkFormat Format)
{
    if (const auto It = m_Pipelines.find(Format); It != std::end(m_Pipelines))
    {
        return *It->second;
    }

    const GraphicsPipelineDesc Desc{.Vertex       = {.Code = shaders::ImGuiVertexShader()},
                                    .Fragment     = {.Code = shaders::ImGuiFragmentShader()},
                                    .ColorFormats = {Format},
                                    .Blend        = {BlendMode::Alpha},
                                    .DepthFormat  = VK_FORMAT_UNDEFINED,
                                    .VertexBindings = {{.binding   = 0U,
                                                        .stride    = static_cast<std::uint32_t>(sizeof(ImDrawVert)),
                                                        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
                                    .VertexAttributes = {{.location = 0U,
                                                          .binding  = 0U,
                                                          .format   = VK_FORMAT_R32G32_SFLOAT,
                                                          .offset   = static_cast<std::uint32_t>(offsetof(ImDrawVert, pos))},
                                                         {.location = 1U,
                                                          .binding  = 0U,
                                                          .format   = VK_FORMAT_R32G32_SFLOAT,
                                                          .offset   = static_cast<std::uint32_t>(offsetof(ImDrawVert, uv))},
                                                         {.location = 2U,
                                                          .binding  = 0U,
                                                          .format   = VK_FORMAT_R8G8B8A8_UNORM,
                                                          .offset   = static_cast<std::uint32_t>(offsetof(ImDrawVert, col))}},
                                    .CullMode     = VK_CULL_MODE_NONE,
                                    .DepthTest    = false,
                                    .DepthWrite   = false,
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
    const ImGuiGeometry Geometry{.Vertices = Allocator.Allocate(static_cast<VkDeviceSize>(DrawData.TotalVtxCount) * sizeof(ImDrawVert)),
                                 .Indices  = Allocator.Allocate(static_cast<VkDeviceSize>(DrawData.TotalIdxCount) * sizeof(ImDrawIdx))};

    VkDeviceSize VertexOffset = 0U;
    VkDeviceSize IndexOffset  = 0U;
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
                                              const VkExtent2D TargetExtent,
                                              const VkFormat Format)
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
    Cmd.BindVertexBuffer(0U, Geometry.Vertices);
    Cmd.BindIndexBuffer(Geometry.Indices, sizeof(ImDrawIdx) == 2U ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32);

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

                Cmd.SetScissor({.offset = {static_cast<std::int32_t>(ClipMin.x), static_cast<std::int32_t>(ClipMin.y)},
                                .extent = {static_cast<std::uint32_t>(ClipMax.x - ClipMin.x), static_cast<std::uint32_t>(ClipMax.y - ClipMin.y)}});

                const std::uint32_t Sampled = detail::ToSampledIndex(Command.GetTexID());
                const std::uint32_t Texture = m_ValidIndices.contains(Sampled) ? Sampled : m_WhiteIndex;

                const PushConstants Push{.Scale = {Scale.x, Scale.y}, .Translate = {Translate.x, Translate.y}, .Texture = Texture, .Sampler = m_Sampler.Index()};
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

    Cmd.SetScissor({.offset = {0, 0}, .extent = TargetExtent});
}



