// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/Composite.hpp"
#include <type_traits>
#include <luvk/Core/Context.hpp>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Render/RenderGraph.hpp>
#include <luvk/Resources/Sampler.hpp>
#include "Shaders/EmbeddedShaders.hpp"
#include "luvk/App/Scene.hpp"

namespace
{
    struct CompositePushConstants
    {
        std::uint32_t Texture    = 0U;
        std::uint32_t Sampler    = 0U;
        std::uint32_t EncodeSrgb = 0U;
    };

    static_assert(std::is_trivially_copyable_v<CompositePushConstants>);
    static_assert(sizeof(CompositePushConstants) <= luvk::PushConstantSize);

    bool IsSrgb(const VkFormat Format)
    {
        return Format == VK_FORMAT_B8G8R8A8_SRGB || Format == VK_FORMAT_R8G8B8A8_SRGB || Format == VK_FORMAT_A8B8G8R8_SRGB_PACK32;
    }
}

luvk::app::CompositeFeature::CompositeFeature(const Sampler& LinearSampler)
    : m_Sampler(&LinearSampler)
{
}

void luvk::app::CompositeFeature::OnAttach(Context& Ctx)
{
    m_Context = &Ctx;
}

void luvk::app::CompositeFeature::OnDetach(Context&)
{
    m_Pipeline.Reset();
    m_Format = VK_FORMAT_UNDEFINED;
}

void luvk::app::CompositeFeature::AddPasses(FeatureContext& Frame)
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
                              {.Vertex       = {.Code = shaders::CompositeVertexShader()},
                               .Fragment     = {.Code = shaders::CompositeFragmentShader()},
                               .ColorFormats = {Frame.Settings.OutputFormat},
                               .CullMode     = VK_CULL_MODE_NONE,
                               .DepthTest    = false,
                               .DepthWrite   = false,
                               .Name         = "luvk composite"});
        m_Format = Frame.Settings.OutputFormat;
    }

    Frame.Graph.AddPass("Composite",
                        PassType::Graphics,
                        [&](PassBuilder& Pass)
                        {
                            Pass.Use(SceneColor, Access::ShaderSampled);
                            Pass.ColorAttachment(Backbuffer, LoadOp::DontCare);
                        },
                        [this, SceneColor](CommandList& Cmd)
                        {
                            const CompositePushConstants Push{.Texture    = Cmd.ResolveTexture(SceneColor).SampledIndex,
                                                              .Sampler    = m_Sampler->Index(),
                                                              .EncodeSrgb = IsSrgb(m_Format) ? 0U : 1U};
                            Cmd.BindPipeline(m_Pipeline);
                            Cmd.PushConstants(Push);
                            Cmd.Draw(3U);
                        });
}
