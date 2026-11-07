// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/SceneRenderer.hpp"
#include <algorithm>
#include <cmath>
#include <luvk/Core/Context.hpp>
#include <luvk/Render/RenderGraph.hpp>
#include "luvk/App/Composite.hpp"

namespace
{
    VkExtent2D Scaled(const VkExtent2D Extent, const float Scale)
    {
        return {std::max(1U, static_cast<std::uint32_t>(std::lround(static_cast<float>(Extent.width) * Scale))),
                std::max(1U, static_cast<std::uint32_t>(std::lround(static_cast<float>(Extent.height) * Scale)))};
    }

    bool SameExtent(const VkExtent2D A, const VkExtent2D B)
    {
        return A.width == B.width && A.height == B.height;
    }
}

luvk::app::SceneRenderer::SceneRenderer(Context& Ctx, const Sampler& LinearSampler, const float RenderScale)
    : m_Context(Ctx),
      m_Features(Ctx),
      m_RenderScale(std::clamp(RenderScale, 0.25F, 2.F)),
      m_SupportsWireframe(Ctx.EnabledFeatures().Core().fillModeNonSolid != VK_FALSE)
{
    m_Features.Add<CompositeFeature>(LinearSampler);
}

luvk::app::SceneRenderer::~SceneRenderer()
{
    m_Features.Clear();
}

void luvk::app::SceneRenderer::SetRenderScale(const float Scale) noexcept
{
    m_RenderScale = std::clamp(Scale, 0.25F, 2.F);
}

void luvk::app::SceneRenderer::SetWireframe(const bool Enabled) noexcept
{
    m_Settings.Wireframe = Enabled && m_SupportsWireframe;
}

void luvk::app::SceneRenderer::SetReverseDepth(const bool Enabled) noexcept
{
    m_Settings.ReverseDepth = Enabled;
}

void luvk::app::SceneRenderer::Update(const float DeltaTime) const
{
    m_Features.Update(DeltaTime);
}

void luvk::app::SceneRenderer::UpdateSettings(const VkExtent2D OutputExtent, const VkFormat OutputFormat)
{
    m_Settings.OutputExtent = OutputExtent;
    m_Settings.OutputFormat = OutputFormat;
    m_Settings.RenderExtent = Scaled(OutputExtent, m_RenderScale);

    // Upscalers depend on both resolutions and the output format, not only on the render extent.
    const bool Changed = !m_Applied.has_value() || !SameExtent(m_Applied->RenderExtent, m_Settings.RenderExtent) ||
                         !SameExtent(m_Applied->OutputExtent, m_Settings.OutputExtent) || m_Applied->OutputFormat != m_Settings.OutputFormat ||
                         m_Applied->ColorFormat != m_Settings.ColorFormat || m_Applied->DepthFormat != m_Settings.DepthFormat ||
                         m_Applied->ReverseDepth != m_Settings.ReverseDepth;
    if (Changed)
    {
        m_Applied = m_Settings;
        m_Features.Resize(m_Settings);
    }
}

void luvk::app::SceneRenderer::AddPasses(const SceneFrame& Frame, const std::function<void(FeatureContext&)>& Custom)
{
    UpdateSettings(Frame.OutputExtent, Frame.OutputFormat);

    RenderGraph& Graph = Frame.Graph;
    m_Board.Clear();
    m_Board.SetTexture(Slots::Backbuffer, Frame.Backbuffer);

    if (Frame.View != nullptr)
    {
        m_View = *Frame.View;
        m_Board.Set(Slots::View, m_View);
    }

    const TextureHandle SceneColor = Graph.CreateTexture({.Extent = m_Settings.RenderExtent, .Format = m_Settings.ColorFormat, .Name = "SceneColor"});
    const TextureHandle SceneDepth = Graph.CreateTexture({.Extent = m_Settings.RenderExtent, .Format = m_Settings.DepthFormat, .Name = "SceneDepth"});
    m_Board.SetTexture(Slots::SceneColor, SceneColor);
    m_Board.SetTexture(Slots::SceneDepth, SceneDepth);

    Graph.AddPass("Clear Scene",
                  PassType::Graphics,
                  [&](PassBuilder& Pass)
                  {
                      Pass.ColorAttachment(SceneColor, LoadOp::Clear, {.float32 = {0.F, 0.F, 0.F, 1.F}});
                      Pass.DepthAttachment(SceneDepth, LoadOp::Clear, m_Settings.ClearDepth());
                  },
                  nullptr);

    FeatureContext Context{.Ctx         = m_Context,
                           .Graph       = Graph,
                           .Allocator   = Frame.Allocator,
                           .Board       = m_Board,
                           .Settings    = m_Settings,
                           .DeltaTime   = Frame.DeltaTime,
                           .Time        = Frame.Time,
                           .FrameNumber = Frame.FrameNumber};

    m_Features.AddPasses(Context, FeatureStage::Prepare, FeatureStage::Overlay);
    if (Custom)
    {
        Custom(Context);
    }
    m_Features.AddPasses(Context, FeatureStage::Output, FeatureStage::Output);
}
