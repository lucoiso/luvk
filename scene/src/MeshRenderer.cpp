// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Scene/MeshRenderer.hpp"
#include <algorithm>
#include <type_traits>
#include <luvk/Core/Context.hpp>
#include <luvk/Render/CommandList.hpp>
#include "luvk/Scene/Mesh.hpp"
#include "Shaders/SceneShaders.hpp"

namespace
{
    /** Per-frame constants, read by the shaders through a buffer device address (matches Mesh.vert / Mesh.frag). */
    struct FrameConstants
    {
        glm::mat4 ViewProjection{1.F};
        glm::vec4 CameraPosition{0.F};
        glm::vec4 LightDirection{0.F};
        glm::vec4 LightColor{0.F};
    };

    /** Per-draw push constants (matches Mesh.vert / Mesh.frag). */
    struct MeshPushConstants
    {
        VkDeviceAddress Frame     = 0U;
        VkDeviceAddress Vertices  = 0U;
        VkDeviceAddress Instances = 0U;
        std::uint32_t   Texture   = 0U;
        std::uint32_t   Sampler   = 0U;
        glm::vec4       BaseColor{1.F};
        glm::vec4       Emissive{0.F};
        std::uint32_t   Flags = 0U;
    };

    static_assert(std::is_trivially_copyable_v<MeshPushConstants>);
    static_assert(sizeof(MeshPushConstants) <= luvk::PushConstantSize);

    /** Material flags (matches Mesh.frag). */
    constexpr std::uint32_t FlagLit     = 1U;
    constexpr std::uint32_t FlagTexture = 2U;

    /** One recorded draw: everything the execute callback needs, resolved on the CPU beforehand. */
    struct PreparedDraw
    {
        const luvk::Pipeline* Pipeline  = nullptr;
        const luvk::Buffer*   Indices   = nullptr;
        luvk::scene::Submesh  Range     = {};
        std::uint32_t         Instances = 0U;
        MeshPushConstants     Push;
    };
}

luvk::scene::MeshRenderer::MeshRenderer(Context& Ctx)
    : m_Context(Ctx),
      m_Sampler(Ctx, {.Name = "luvk-scene linear repeat"})
{
}

void luvk::scene::MeshRenderer::Draw(const Mesh& Target, const Material& Surface, const glm::mat4& Transform)
{
    DrawInstanced(Target, Surface, std::span<const glm::mat4>(&Transform, 1U));
}

void luvk::scene::MeshRenderer::DrawInstanced(const Mesh& Target, const Material& Surface, const std::span<const glm::mat4> Transforms)
{
    if (!Target || std::empty(Transforms))
    {
        return;
    }

    m_Requests.push_back({.Target         = &Target,
                          .Surface        = Surface,
                          .FirstTransform = static_cast<std::uint32_t>(std::size(m_Transforms)),
                          .TransformCount = static_cast<std::uint32_t>(std::size(Transforms))});
    m_Transforms.insert(std::end(m_Transforms), std::begin(Transforms), std::end(Transforms));
}

const luvk::Pipeline& luvk::scene::MeshRenderer::PipelineFor(const PipelineKey& Key)
{
    if (const auto It = std::ranges::find(m_Pipelines, Key, &std::pair<PipelineKey, std::unique_ptr<Pipeline>>::first); It != std::end(m_Pipelines))
    {
        return *It->second;
    }

    const GraphicsPipelineDesc Desc{.Vertex       = {.Code = shaders::MeshVertexShader()},
                                    .Fragment     = {.Code = shaders::MeshFragmentShader()},
                                    .ColorFormats = {Key.ColorFormat},
                                    .Blend        = {Key.Transparent ? BlendMode::Alpha : BlendMode::Opaque},
                                    .DepthFormat  = Key.DepthFormat,
                                    .PolygonMode  = Key.Wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
                                    .CullMode     = static_cast<VkCullModeFlags>(Key.DoubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT),
                                    .DepthTest    = true,
                                    .DepthWrite   = !Key.Transparent,
                                    .DepthCompare = Key.ReverseDepth ? VK_COMPARE_OP_GREATER_OR_EQUAL : VK_COMPARE_OP_LESS_OR_EQUAL,
                                    .Name         = "luvk-scene mesh"};

    return *m_Pipelines.emplace_back(Key, std::make_unique<Pipeline>(m_Context, Desc)).second;
}

void luvk::scene::MeshRenderer::AddPasses(RenderGraph& Graph, FrameAllocator& Allocator, const MeshTargets& Targets)
{
    m_LastDrawCount     = 0U;
    m_LastInstanceCount = static_cast<std::uint32_t>(std::size(m_Transforms));

    if (std::empty(m_Requests) || !Targets.Color.IsValid() || !Targets.Depth.IsValid())
    {
        m_Requests.clear();
        m_Transforms.clear();
        return;
    }

    const FrameConstants Constants{.ViewProjection = Targets.ViewProjection,
                                   .CameraPosition = glm::vec4(Targets.CameraPosition, 1.F),
                                   .LightDirection = glm::vec4(glm::normalize(Light.Direction), Light.Ambient),
                                   .LightColor     = glm::vec4(Light.Color, 1.F)};

    const FrameAllocation Frame     = Allocator.Upload(std::span<const FrameConstants>(&Constants, 1U));
    const FrameAllocation Instances = Allocator.Upload(std::span<const glm::mat4>(m_Transforms));

    // Opaque meshes first, then the transparent ones over them (in queue order).
    std::ranges::stable_partition(m_Requests,
                                  [](const DrawRequest& Request)
                                  {
                                      return !Request.Surface.Transparent;
                                  });

    // Every material image is a graph resource sampled by the pass, so earlier writes this frame are synchronised.
    std::vector<TextureHandle> Sampled;
    for (const DrawRequest& Request : m_Requests)
    {
        if (Request.Surface.BaseColorTexture != nullptr)
        {
            Sampled.push_back(Graph.Import(*Request.Surface.BaseColorTexture));
        }
    }

    std::vector<PreparedDraw> Draws;
    for (const DrawRequest& Request : m_Requests)
    {
        const Material&   Surface  = Request.Surface;
        const Pipeline&   Selected = PipelineFor({.ColorFormat  = Targets.ColorFormat,
                                                  .DepthFormat  = Targets.DepthFormat,
                                                  .Wireframe    = Targets.Wireframe,
                                                  .ReverseDepth = Targets.ReverseDepth,
                                                  .Transparent  = Surface.Transparent,
                                                  .DoubleSided  = Surface.DoubleSided});
        const bool        Textured = Surface.BaseColorTexture != nullptr;

        const MeshPushConstants Push{.Frame     = Frame.Address,
                                     .Vertices  = Request.Target->Vertices().Address(),
                                     .Instances = Instances.Address + static_cast<VkDeviceAddress>(Request.FirstTransform) * sizeof(glm::mat4),
                                     .Texture   = Textured ? Surface.BaseColorTexture->SampledIndex() : 0U,
                                     .Sampler   = Surface.Sampler != InvalidBindlessIndex ? Surface.Sampler : m_Sampler.Index(),
                                     .BaseColor = Surface.BaseColor,
                                     .Emissive  = glm::vec4(Surface.Emissive, Surface.Roughness),
                                     .Flags     = (Surface.Model == Shading::Lit ? FlagLit : 0U) | (Textured ? FlagTexture : 0U)};

        for (const Submesh& Range : Request.Target->Submeshes())
        {
            Draws.push_back({.Pipeline  = &Selected,
                             .Indices   = &Request.Target->Indices(),
                             .Range     = Range,
                             .Instances = Request.TransformCount,
                             .Push      = Push});
        }
    }

    m_LastDrawCount = static_cast<std::uint32_t>(std::size(Draws));
    m_Requests.clear();
    m_Transforms.clear();

    Graph.AddPass("Meshes",
                  PassType::Graphics,
                  [&](PassBuilder& Pass)
                  {
                      Pass.ColorAttachment(Targets.Color, LoadOp::Load);
                      Pass.DepthAttachment(Targets.Depth, LoadOp::Load);
                      for (const TextureHandle Texture : Sampled)
                      {
                          Pass.Use(Texture, Access::ShaderSampled);
                      }
                  },
                  [Draws = std::move(Draws)](CommandList& Cmd)
                  {
                      for (const PreparedDraw& It : Draws)
                      {
                          Cmd.BindPipeline(*It.Pipeline);
                          Cmd.BindIndexBuffer(*It.Indices, VK_INDEX_TYPE_UINT32);
                          Cmd.PushConstants(It.Push);
                          Cmd.DrawIndexed(It.Range.IndexCount, It.Instances, It.Range.FirstIndex);
                      }
                  });
}
