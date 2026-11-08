// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Frame/SceneRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <luvk/Core/Context.hpp>
#include <luvk/Core/FeatureChain.hpp>
#include <luvk/Render/RenderGraph.hpp>
#include "luvk/Frame/StandardFeatures.hpp"

namespace
{
    luvk::Extent2D Scaled(const luvk::Extent2D Extent, const float Scale)
    {
        return {std::max(1U, static_cast<std::uint32_t>(std::lround(static_cast<float>(Extent.Width) * Scale))),
                std::max(1U, static_cast<std::uint32_t>(std::lround(static_cast<float>(Extent.Height) * Scale)))};
    }

    bool SameRect(const luvk::Rect2D& A, const luvk::Rect2D& B)
    {
        return A.Offset.X == B.Offset.X && A.Offset.Y == B.Offset.Y && A.Extent == B.Extent;
    }
}

luvk::frame::SceneRenderer::SceneRenderer(Context& Ctx, const Sampler& LinearSampler, const float RenderScale)
    : m_Context(Ctx),
      m_Features(Ctx),
      m_RenderScale(std::clamp(RenderScale, 0.25F, 2.F)),
      m_SupportsWireframe(Ctx.EnabledFeatures().Core().fillModeNonSolid != VK_FALSE)
{
    m_Features.Add<SceneClearFeature>();
    m_Features.Add<CompositeFeature>(LinearSampler);
}

luvk::frame::SceneRenderer::~SceneRenderer()
{
    m_Features.Clear();
}

void luvk::frame::SceneRenderer::SetRenderScale(const float Scale) noexcept
{
    m_RenderScale = std::clamp(Scale, 0.25F, 2.F);
}

void luvk::frame::SceneRenderer::SetWireframe(const bool Enabled) noexcept
{
    m_Settings.Wireframe = Enabled && m_SupportsWireframe;
}

void luvk::frame::SceneRenderer::SetReverseDepth(const bool Enabled) noexcept
{
    m_Settings.ReverseDepth = Enabled;
}

void luvk::frame::SceneRenderer::SetSceneFormats(const Format Color, const Format Depth) noexcept
{
    m_Settings.ColorFormat = Color;
    m_Settings.DepthFormat = Depth;
}

void luvk::frame::SceneRenderer::Update(const float DeltaTime) const
{
    m_Features.Update(DeltaTime);
}

void luvk::frame::SceneRenderer::UpdateSettings(const Extent2D OutputExtent, const Format OutputFormat, const Rect2D Viewport)
{
    Rect2D Clamped = Viewport;
    if (Clamped.Extent.Width == 0U || Clamped.Extent.Height == 0U)
    {
        Clamped = Rect2D{.Offset = {0, 0}, .Extent = OutputExtent};
    }

    const std::int32_t MaxX = static_cast<std::int32_t>(OutputExtent.Width);
    const std::int32_t MaxY = static_cast<std::int32_t>(OutputExtent.Height);
    const std::int32_t X    = std::clamp(Clamped.Offset.X, 0, MaxX);
    const std::int32_t Y    = std::clamp(Clamped.Offset.Y, 0, MaxY);
    const std::int32_t W    = std::clamp(static_cast<std::int32_t>(Clamped.Extent.Width), 0, MaxX - X);
    const std::int32_t H    = std::clamp(static_cast<std::int32_t>(Clamped.Extent.Height), 0, MaxY - Y);
    Clamped                 = Rect2D{.Offset = {X, Y}, .Extent = {static_cast<std::uint32_t>(W), static_cast<std::uint32_t>(H)}};

    m_Settings.OutputExtent = OutputExtent;
    m_Settings.OutputFormat = OutputFormat;
    m_Settings.Viewport     = Clamped;
    m_Settings.RenderExtent = Scaled(Clamped.Extent, m_RenderScale);

    // Upscalers depend on both resolutions and the output format, not only on the render extent.
    const bool Changed = !m_Applied.has_value() || m_Applied->RenderExtent != m_Settings.RenderExtent || m_Applied->OutputExtent != m_Settings.OutputExtent ||
                         !SameRect(m_Applied->Viewport, m_Settings.Viewport) || m_Applied->OutputFormat != m_Settings.OutputFormat ||
                         m_Applied->ColorFormat != m_Settings.ColorFormat || m_Applied->DepthFormat != m_Settings.DepthFormat ||
                         m_Applied->ReverseDepth != m_Settings.ReverseDepth;
    if (Changed)
    {
        m_Applied = m_Settings;
        m_Features.Resize(m_Settings);
    }
}

void luvk::frame::SceneRenderer::AddPasses(const SceneFrame& Frame, const std::function<void(FeatureContext&)>& Custom)
{
    UpdateSettings(Frame.OutputExtent, Frame.OutputFormat, Frame.Viewport);
    m_Settings.ProjectionJitter = Frame.ProjectionJitter;

    RenderGraph& Graph = Frame.Graph;
    m_Board.Clear();
    m_Board.Set(Slots::Backbuffer, Frame.Backbuffer);

    if (Frame.View != nullptr)
    {
        m_View = *Frame.View;
        m_Board.Set(Slots::View, m_View);
    }

    m_Board.Set(Slots::SceneColor, Graph.CreateTexture({.Extent = m_Settings.RenderExtent, .Format = m_Settings.ColorFormat, .Name = "SceneColor"}));
    m_Board.Set(Slots::SceneDepth, Graph.CreateTexture({.Extent = m_Settings.RenderExtent, .Format = m_Settings.DepthFormat, .Name = "SceneDepth"}));

    FeatureContext Context{.Ctx         = m_Context,
                           .Graph       = Graph,
                           .AsyncGraph  = Frame.AsyncGraph != nullptr ? *Frame.AsyncGraph : Graph,
                           .Allocator   = Frame.Allocator,
                           .Board       = m_Board,
                           .Settings    = m_Settings,
                           .DeltaTime   = Frame.DeltaTime,
                           .Time        = Frame.Time,
                           .FrameNumber = Frame.FrameNumber};

    m_Features.AddPasses(Context, Phases::Prepare, Phases::Overlay);
    if (Custom)
    {
        Custom(Context);
    }
    m_Features.AddPasses(Context, Phases::Output, Phases::Output);
}
