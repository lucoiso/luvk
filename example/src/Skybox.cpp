// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include <type_traits>
#include <array>
#include <cmath>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Shaders/ShaderCompiler.hpp>
#include <luvk/App/Camera.hpp>
#include "Shaders/Skybox.hpp"
#include "Skybox.hpp"

namespace
{

    using Shaders::Skybox::Shader;

    glm::vec3 EvaluateSky(const glm::vec3& Direction)
    {
        constexpr glm::vec3 Bottom(0.16F, 0.19F, 0.24F);
        constexpr glm::vec3 Top(0.30F, 0.52F, 0.88F);
        constexpr glm::vec3 SunColor(1.0F, 0.92F, 0.75F);
        const glm::vec3     SunDirection = glm::normalize(glm::vec3{0.35F, 0.55F, -0.75F});

        const float Gradient = glm::clamp(Direction.y * 0.5F + 0.5F, 0.F, 1.F);
        glm::vec3   Color    = glm::mix(Bottom, Top, Gradient);
        Color += SunColor * std::pow(glm::max(glm::dot(Direction, SunDirection), 0.F), 48.F) * 0.9F;
        return glm::clamp(Color, 0.F, 1.F);
    }

    /** Appends one cube face of the procedural sky (RGBA8) to Pixels. */
    void GenerateFace(const std::uint32_t Face, const std::uint32_t Size, std::vector<std::byte>& Pixels)
    {

        for (std::uint32_t Y = 0U; Y < Size; ++Y)
        {
            for (std::uint32_t X = 0U; X < Size; ++X)
            {
                const float U = (static_cast<float>(X) + 0.5F) / static_cast<float>(Size) * 2.F - 1.F;
                const float V = (static_cast<float>(Y) + 0.5F) / static_cast<float>(Size) * 2.F - 1.F;

                glm::vec3 Direction;
                switch (Face)
                {
                case 0U: Direction = {1.F, -V, -U}; break;
                case 1U: Direction = {-1.F, -V, U}; break;
                case 2U: Direction = {U, 1.F, V}; break;
                case 3U: Direction = {U, -1.F, -V}; break;
                case 4U: Direction = {U, -V, 1.F}; break;
                default: Direction = {-U, -V, -1.F}; break;
                }

                const glm::vec3 Color = EvaluateSky(glm::normalize(Direction));
                Pixels.push_back(static_cast<std::byte>(Color.r * 255.F + 0.5F));
                Pixels.push_back(static_cast<std::byte>(Color.g * 255.F + 0.5F));
                Pixels.push_back(static_cast<std::byte>(Color.b * 255.F + 0.5F));
                Pixels.push_back(std::byte{255U});
            }
        }
    }
}

Example::Skybox::Skybox(const std::uint32_t SamplerIndex)
    : m_Sampler(SamplerIndex)
{
}

void Example::Skybox::OnAttach(luvk::Context& Ctx)
{
    constexpr std::uint32_t Size = 128U;

    std::vector<std::byte> Faces;
    Faces.reserve(static_cast<std::size_t>(Size) * Size * 4U * 6U);
    for (std::uint32_t Face = 0U; Face < 6U; ++Face)
    {
        GenerateFace(Face, Size, Faces);
    }

    m_Cubemap = luvk::scene::Texture::CubeFromFaces(Ctx, Faces, Size, {.GenerateMips = false, .Name = "sky cubemap"});
}

void Example::Skybox::AddPasses(luvk::app::FeatureContext& Frame)
{
    static_assert(std::is_trivially_copyable_v<PushData>);
    static_assert(sizeof(PushData) <= luvk::PushConstantSize);
    if (m_Format != Frame.Settings.ColorFormat)
    {
        const auto Vertex   = luvk::CompileShaderOrThrow(Shader, "skybox", "VertexMain");
        const auto Fragment = luvk::CompileShaderOrThrow(Shader, "skybox", "FragmentMain");
        m_Pipeline          = luvk::Pipeline(Frame.Ctx,
                                             luvk::GraphicsPipelineDesc{.Vertex       = {.Code = Vertex},
                                                                        .Fragment     = {.Code = Fragment},
                                                                        .ColorFormats = {Frame.Settings.ColorFormat},
                                                                        .CullMode     = VK_CULL_MODE_NONE,
                                                                        .DepthTest    = false,
                                                                        .DepthWrite   = false,
                                                                        .Name         = "skybox"});
        m_Format = Frame.Settings.ColorFormat;
    }

    const auto* const View = Frame.Board.Find<luvk::app::ViewData>(luvk::app::Slots::View);
    if (View == nullptr)
    {
        return;
    }

    const PushData Push{.InverseViewProjection = View->InverseViewProjection,
                        .CameraPosition        = View->Position,
                        .Texture               = m_Cubemap.SampledIndex(),
                        .Sampler               = m_Sampler};

    const luvk::TextureHandle Color   = Frame.Board.FindTexture(luvk::app::Slots::SceneColor);
    const luvk::TextureHandle Cubemap = Frame.Graph.Import(m_Cubemap.GetImage());

    Frame.Graph.AddPass("Skybox",
                        luvk::PassType::Graphics,
                        [&](luvk::PassBuilder& Pass)
                        {
                            Pass.Use(Cubemap, luvk::Access::ShaderSampled);
                            Pass.ColorAttachment(Color, luvk::LoadOp::Load);
                        },
                        [this, Push](luvk::CommandList& Cmd)
                        {
                            Cmd.BindPipeline(m_Pipeline);
                            Cmd.PushConstants(Push);
                            Cmd.Draw(3U);
                        });
}
