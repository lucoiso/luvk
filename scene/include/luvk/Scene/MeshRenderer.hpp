// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>
#include <glm/glm.hpp>
#include <luvk/Render/FrameAllocator.hpp>
#include <luvk/Render/RenderGraph.hpp>
#include <luvk/Resources/Pipeline.hpp>
#include <luvk/Resources/Sampler.hpp>
#include "luvk/Scene/Material.hpp"

namespace luvk
{
    class Context;
}

namespace luvk::scene
{
    class Mesh;

    /** The single light of the MeshRenderer's lit materials. */
    struct DirectionalLight
    {
        /** World-space direction the light travels in. */
        glm::vec3 Direction{-0.4F, -1.F, -0.3F};

        /** Linear light color (intensity included). */
        glm::vec3 Color{1.F};

        /** Constant ambient term applied to lit materials. */
        float Ambient = 0.12F;
    };

    /** Where and how the MeshRenderer draws this frame. */
    struct MeshTargets
    {
        /** Color attachment (loaded, not cleared). */
        TextureHandle Color = {};

        /** Depth attachment (loaded, not cleared). */
        TextureHandle Depth = {};

        /** Format of Color. */
        VkFormat ColorFormat = VK_FORMAT_UNDEFINED;

        /** Format of Depth. */
        VkFormat DepthFormat = VK_FORMAT_UNDEFINED;

        /** Camera view-projection matrix. */
        glm::mat4 ViewProjection{1.F};

        /** Camera world-space position (specular highlights). */
        glm::vec3 CameraPosition{0.F};

        /** Rasterizes lines instead of filled triangles. */
        bool Wireframe = false;

        /** Reversed depth (near = 1, far = 0): meshes compare with GREATER_OR_EQUAL. */
        bool ReverseDepth = false;
    };

    /**
     * Draws meshes with materials. Draw / DrawInstanced queue work during the frame; AddPasses records it as one graph
     * pass (opaque first, then transparent) and clears the queue. Instance transforms and per-frame constants live in
     * the frame's FrameAllocator, vertices are pulled through buffer device addresses: there is nothing to bind or
     * synchronise by hand. Pipelines are created on first use per target format and material state.
     */
    class MeshRenderer
    {
    public:
        /** Constructor. Creates the default linear repeat sampler. */
        explicit MeshRenderer(Context& Ctx);

        /** Non-copyable. */
        MeshRenderer(const MeshRenderer&) = delete;

        /** Non-copyable. */
        MeshRenderer& operator=(const MeshRenderer&) = delete;

        /** Queues one instance of Target (it must stay alive until the frame is recorded). */
        void Draw(const Mesh& Target, const Material& Surface, const glm::mat4& Transform);

        /** Queues one instance of Target per transform, drawn with a single instanced draw per submesh. */
        void DrawInstanced(const Mesh& Target, const Material& Surface, std::span<const glm::mat4> Transforms);

        /** Records the queued draws into Graph and clears the queue (no pass when nothing was queued). */
        void AddPasses(RenderGraph& Graph, FrameAllocator& Allocator, const MeshTargets& Targets);

        /** Light used by lit materials. */
        DirectionalLight Light;

        /** Draw calls recorded by the last AddPasses. */
        [[nodiscard]] std::uint32_t LastDrawCount() const noexcept { return m_LastDrawCount; }

        /** Instances recorded by the last AddPasses. */
        [[nodiscard]] std::uint32_t LastInstanceCount() const noexcept { return m_LastInstanceCount; }

    private:
        /** One queued Draw / DrawInstanced call. */
        struct DrawRequest
        {
            /** Mesh drawn. */
            const Mesh* Target = nullptr;

            /** Material copied at queue time. */
            Material Surface;

            /** First transform in m_Transforms. */
            std::uint32_t FirstTransform = 0U;

            /** Number of transforms (instances). */
            std::uint32_t TransformCount = 0U;
        };

        /** Pipeline state derived from the targets and the material. */
        struct PipelineKey
        {
            /** Color attachment format. */
            VkFormat ColorFormat = VK_FORMAT_UNDEFINED;

            /** Depth attachment format. */
            VkFormat DepthFormat = VK_FORMAT_UNDEFINED;

            /** Line rasterization. */
            bool Wireframe = false;

            /** Reversed depth comparison. */
            bool ReverseDepth = false;

            /** Alpha blending without depth writes. */
            bool Transparent = false;

            /** No back-face culling. */
            bool DoubleSided = false;

            /** Field-wise equality. */
            [[nodiscard]] bool operator==(const PipelineKey&) const = default;
        };

        /** Returns (creating on first use) the pipeline for Key. */
        const Pipeline& PipelineFor(const PipelineKey& Key);

        /** Owning context. */
        Context& m_Context;

        /** Linear repeat sampler used when a material has none. */
        Sampler m_Sampler;

        /** Pipelines created so far. */
        std::vector<std::pair<PipelineKey, std::unique_ptr<Pipeline>>> m_Pipelines;

        /** Draws queued for the frame. */
        std::vector<DrawRequest> m_Requests;

        /** Instance transforms of the queued draws. */
        std::vector<glm::mat4> m_Transforms;

        /** Draw calls recorded by the last AddPasses. */
        std::uint32_t m_LastDrawCount = 0U;

        /** Instances recorded by the last AddPasses. */
        std::uint32_t m_LastInstanceCount = 0U;
    };
}
