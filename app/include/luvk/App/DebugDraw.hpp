// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <glm/glm.hpp>
#include <luvk/Resources/Pipeline.hpp>
#include <luvk/Frame/Feature.hpp>

namespace luvk::app
{
    /**
     * Header of the GPU debug line buffer (Slots::DebugLines). The first four words are the VkDrawIndirectCommand
     * DebugDraw draws with; GpuDebugLineVertex records follow the header. Shaders append a line with
     * `uint First = atomicAdd(VertexCount, 2); if (First + 2 <= Capacity) { write Vertices[First], Vertices[First + 1]; }`.
     */
    struct GpuDebugLinesHeader
    {
        /** Vertices reserved so far (2 per line); may exceed Capacity, extra vertices are not drawn. */
        std::uint32_t VertexCount = 0U;

        /** Always 1. */
        std::uint32_t InstanceCount = 1U;

        /** Always 0. */
        std::uint32_t FirstVertex = 0U;

        /** Always 0. */
        std::uint32_t FirstInstance = 0U;

        /** Number of vertices the buffer holds after the header. */
        std::uint32_t Capacity = 0U;

        /** Keeps the vertices 16-byte aligned. */
        std::uint32_t Padding[3] = {};
    };

    /** One debug line endpoint: world-space position and RGBA8 color (red in the lowest byte). */
    struct GpuDebugLineVertex
    {
        /** Endpoint position in world space. */
        float Position[3] = {};

        /** Packed RGBA color, one byte per channel. */
        std::uint32_t Color = 0U;
    };

    static_assert(sizeof(GpuDebugLinesHeader) == 32U, "GpuDebugLinesHeader is read by shaders: keep it 32 bytes.");
    static_assert(sizeof(GpuDebugLineVertex) == 16U, "GpuDebugLineVertex is read by shaders: keep it 16 bytes.");

    /** Immediate-mode world-space lines (boxes, frustums, grids, axes...). Submit shapes any time during the frame.
     *  They are drawn into SceneColor (depth-tested against SceneDepth, without writing it) and cleared; vertices are
     *  uploaded to a per-frame vertex buffer that the deletion queue frees once the GPU is done.
     *
     *  GPU passes (culling, meshing...) can draw lines too, without readback: AcquireGpuLines returns a per-frame buffer
     *  they append to, and DebugDraw draws its contents indirectly in the same frame. */
    class DebugDraw final : public frame::RenderFeature
    {
    public:
        /** Toggles depth testing of the debug lines against SceneDepth. */
        bool DepthTest = true;

        /** Returns the feature name shown in the stats window. */
        [[nodiscard]] std::string_view Name() const override { return "luvk.DebugDraw"; }

        /** Runs in the Debug phase of the frame, after most scene features. */
        [[nodiscard]] std::string_view Phase() const override { return frame::Phases::Debug; }

        /** Uploads the submitted vertices and records the line drawing pass into the frame graph. */
        void AddPasses(frame::FeatureContext& Frame) override;

        /** Draws the depth test toggle and the vertex count of the last frame. */
        void OnGui() override;

        /** Appends a line segment between two world-space points. */
        void Line(const glm::vec3& From, const glm::vec3& To, const glm::vec4& Color);

        /** Appends the 12 edges of the axis-aligned box (Min, Max) transformed by Transform (identity by default). */
        void Box(const glm::vec3& Min, const glm::vec3& Max, const glm::vec4& Color, const glm::mat4& Transform = glm::mat4{1.F});

        /** Appends the 12 edges of the frustum whose corners are reconstructed by InverseViewProjection. */
        void Frustum(const glm::mat4& InverseViewProjection, const glm::vec4& Color);

        /** Appends a grid of Divisions x Divisions cells in the XZ plane, centered on the origin. */
        void Grid(float HalfSize, int Divisions, const glm::vec4& Color);

        /** Appends three lines along the transform's basis vectors (X red, Y green, Z blue) from its origin. */
        void Axes(const glm::mat4& Transform, float Size);

        /** Appends a circle of Segments line segments around Center, facing Normal. */
        void Circle(const glm::vec3& Center, const glm::vec3& Normal, float Radius, const glm::vec4& Color, int Segments = 32);

        /**
         * Returns the frame's GPU debug line buffer (GpuDebugLinesHeader + MaxLines * 2 GpuDebugLineVertex), creating and
         * resetting it on the first call of the frame (later calls ignore MaxLines). Any feature that runs before the
         * Debug stage can call it without a reference to the DebugDraw instance: the buffer is published as
         * Slots::DebugLines. Declare it with Access::ShaderStorageWrite in the passes that append, and pass its
         * address (CommandList::ResolveBuffer(...).Address) to the shaders.
         */
        [[nodiscard]] static BufferHandle AcquireGpuLines(frame::FeatureContext& Frame, std::uint32_t MaxLines = 65536U);

    private:
        /** Line endpoints submitted since the last AddPasses (the GPU line layout); uploaded and cleared every frame. */
        std::vector<GpuDebugLineVertex> m_Vertices;

        /** Line pipeline with depth test enabled. */
        Pipeline m_Tested;

        /** Line pipeline with depth test disabled. */
        Pipeline m_Overlay;

        /** GPU line pipeline (vertex pulling, indirect) with depth test enabled. */
        Pipeline m_GpuTested;

        /** GPU line pipeline with depth test disabled. */
        Pipeline m_GpuOverlay;

        /** True when a GPU line buffer was drawn in the previous frame, shown in OnGui. */
        bool m_LastGpuLines = false;

        /** SceneColor format the pipelines were built for; rebuilt on change. */
        Format m_ColorFormat = Format::Undefined;

        /** SceneDepth format the pipelines were built for; rebuilt on change. */
        Format m_DepthFormat = Format::Undefined;

        /** Depth comparison the tested pipeline was built with (follows FrameSettings::ReverseDepth). */
        CompareOp m_DepthCompare = CompareOp::LessOrEqual;

        /** Number of vertices drawn in the previous frame, shown in OnGui. */
        std::uint32_t m_LastVertexCount = 0U;

        /** Builds the tested/overlay pipelines for the given formats. */
        void CreatePipelines(Context& Ctx, Format ColorFormat, Format DepthFormat, CompareOp DepthCompare);
    };
}
