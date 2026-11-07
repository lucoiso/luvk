// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include <stdexcept>
#include <luvk/Core/Context.hpp>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Shaders/ShaderCompiler.hpp>
#include "Shaders/ShaderPlayground.hpp"
#include "ShaderPlayground.hpp"

namespace
{

    using Shaders::ShaderPlayground::VertexSource;
    using Shaders::ShaderPlayground::DefaultFragmentSource;
    constexpr std::uint32_t PreviewSize = 256U;
}

std::string_view Example::ShaderPlayground::DefaultSource()
{
    return DefaultFragmentSource;
}

void Example::ShaderPlayground::Update(const float DeltaTime)
{
    m_Time += DeltaTime;
}

void Example::ShaderPlayground::OnAttach(luvk::Context& Ctx)
{
    m_Context      = &Ctx;
    m_VertexShader = luvk::CompileShaderOrThrow(VertexSource, "playground_vertex", "main");
    m_Output       = luvk::Image(Ctx,
                                 {.Extent = {PreviewSize, PreviewSize, 1U},
                                  .Format = VK_FORMAT_R8G8B8A8_UNORM,
                                  .Usage  = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                  .Name   = "shader playground"});

    if (const std::string Error = Compile(DefaultFragmentSource);
        !std::empty(Error))
    {
        throw std::runtime_error("Default playground shader failed to compile:\n" + Error);
    }
}

std::string Example::ShaderPlayground::Compile(const std::string_view FragmentSource)
{
    auto Fragment = luvk::CompileShader(FragmentSource, {.ModuleName = "playground", .EntryPoint = "main"});
    if (!Fragment)
    {
        return Fragment.error();
    }

    try
    {

        m_Pipeline = luvk::Pipeline(*m_Context,
                                    luvk::GraphicsPipelineDesc{.Vertex       = {.Code = m_VertexShader},
                                                               .Fragment     = {.Code = *Fragment},
                                                               .ColorFormats = {VK_FORMAT_R8G8B8A8_UNORM},
                                                               .CullMode     = VK_CULL_MODE_NONE,
                                                               .Name         = "shader playground"});
    }
    catch (const std::exception& Exception)
    {
        return Exception.what();
    }

    m_Time = 0.F;
    return {};
}

void Example::ShaderPlayground::AddPasses(luvk::app::FeatureContext& Frame)
{
    const luvk::TextureHandle Output = Frame.Graph.Import(m_Output);
    const float               Time   = m_Time;

    Frame.Graph.AddPass("Shader Playground",
                        luvk::PassType::Graphics,
                        [&](luvk::PassBuilder& Pass)
                        {
                            Pass.ColorAttachment(Output, luvk::LoadOp::Clear, {.float32 = {0.F, 0.F, 0.F, 1.F}});
                        },
                        [this, Time](luvk::CommandList& Cmd)
                        {
                            Cmd.BindPipeline(m_Pipeline);
                            Cmd.PushConstants(Time);
                            Cmd.Draw(3U);
                        });
}
