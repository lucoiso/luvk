// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Frame/StandardFeatures.hpp"
#include <type_traits>
#include <luvk/Core/Context.hpp>
#include <luvk/Core/Handles.hpp>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Render/RenderGraph.hpp>
#include <luvk/Resources/Sampler.hpp>
#include "FrameShaders.hpp"
#include "luvk/Frame/Slots.hpp"

namespace
{
    /** Push constants of Composite.slang. */
    struct CompositePush
    {
        luvk::Texture2D     Source;
        luvk::SamplerHandle Sampler;
        std::uint32_t       EncodeSrgb = 0U;
    };

    static_assert(std::is_trivially_copyable_v<CompositePush> && sizeof(CompositePush) == 12U);

    bool IsSrgb(const luvk::Format Value) noexcept
    {
        return Value == luvk::Format::B8G8R8A8Srgb || Value == luvk::Format::R8G8B8A8Srgb;
    }
}

void luvk::frame::SceneClearFeature::AddPasses(FeatureContext& Frame)
{
    const TextureHandle SceneColor = Frame.Board.FindTexture(Slots::SceneColor);
    const TextureHandle SceneDepth = Frame.Board.FindTexture(Slots::SceneDepth);
    if (!SceneColor.IsValid() || !SceneDepth.IsValid())
    {
        return;
    }

    const ClearColorValue Clear{.Float = {Color[0], Color[1], Color[2], Color[3]}};
    Frame.Graph.AddPass("Clear Scene",
                        PassType::Graphics,
                        [&](PassBuilder& Pass)
                        {
                            Pass.ColorAttachment(SceneColor, LoadOp::Clear, Clear);
                            Pass.DepthAttachment(SceneDepth, LoadOp::Clear, Frame.Settings.ClearDepth());
                        },
                        nullptr);
}

luvk::frame::CompositeFeature::CompositeFeature(const Sampler& LinearSampler)
    : m_Sampler(&LinearSampler)
{
}

void luvk::frame::CompositeFeature::OnAttach(Context& Ctx)
{
    m_Context = &Ctx;
}

void luvk::frame::CompositeFeature::OnDetach(Context&)
{
    m_Pipeline.Reset();
    m_Format = Format::Undefined;
}

void luvk::frame::CompositeFeature::AddPasses(FeatureContext& Frame)
{
    const TextureHandle SceneColor = Frame.Board.FindTexture(Slots::SceneColor);
    const TextureHandle Backbuffer = Frame.Board.FindTexture(Slots::Backbuffer);
    if (!SceneColor.IsValid() || !Backbuffer.IsValid())
    {
        return;
    }

    if (m_Format != Frame.Settings.OutputFormat)
    {
        m_Pipeline = Pipeline(*m_Context,
                              GraphicsPipelineDesc{.Vertex       = {.Code = Shaders::CompositeVertex()},
                                                   .Fragment     = {.Code = Shaders::CompositeFragment()},
                                                   .ColorFormats = {Frame.Settings.OutputFormat},
                                                   .CullMode     = CullMode::None,
                                                   .DepthTest    = false,
                                                   .DepthWrite   = false,
                                                   .PushSize     = sizeof(CompositePush),
                                                   .Name         = "luvk composite"});
        m_Format = Frame.Settings.OutputFormat;
    }

    const Rect2D Viewport = Frame.Settings.Viewport;
    Frame.Graph.AddPass<CompositePush>(
            "Composite",
            PassType::Graphics,
            [&](PassBinder& Bind)
            {
                Bind.ColorAttachment(Backbuffer, LoadOp::Clear, ClearColorValue{.Float = {0.02F, 0.02F, 0.03F, 1.F}});
                return CompositePush{.Source = Bind.Sampled(SceneColor), .Sampler = m_Sampler->Bindless(), .EncodeSrgb = IsSrgb(m_Format) ? 0U : 1U};
            },
            [this, Viewport](CommandList& Cmd, const CompositePush& Push)
            {
                if (Viewport.Extent.Width > 0U && Viewport.Extent.Height > 0U)
                {
                    Cmd.SetViewport(luvk::Viewport{.X      = static_cast<float>(Viewport.Offset.X),
                                                   .Y      = static_cast<float>(Viewport.Offset.Y),
                                                   .Width  = static_cast<float>(Viewport.Extent.Width),
                                                   .Height = static_cast<float>(Viewport.Extent.Height)});
                    Cmd.SetScissor(Viewport);
                }

                Cmd.BindPipeline(m_Pipeline);
                Cmd.PushConstants(Push);
                Cmd.Draw(3U);
            });
}
