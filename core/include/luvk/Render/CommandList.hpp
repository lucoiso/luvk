// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include "luvk/Core/Common.hpp"
#include "luvk/Render/FrameAllocator.hpp"
#include "luvk/Render/RenderGraph.hpp"

namespace luvk
{
    class Context;
    class Pipeline;
    class Buffer;

    /** One region of CommandList::CopyBuffer: Size bytes from SourceOffset to DestinationOffset. */
    struct BufferCopy
    {
        /** Byte offset in the source buffer. */
        DeviceSize SourceOffset = 0U;

        /** Byte offset in the destination buffer. */
        DeviceSize DestinationOffset = 0U;

        /** Number of bytes to copy. */
        DeviceSize Size = 0U;
    };

    /**
     * Thin, zero-overhead helper over a VkCommandBuffer. Binding a pipeline also binds the global bindless
     * set, so a typical draw is: BindPipeline -> PushConstants -> Draw. Handle() is always available for raw
     * Vulkan calls.
     */
    class LUVK_API CommandList
    {
        /** Context providing the bindless layout and debug utils state. */
        Context* m_Context = nullptr;

        /** Command buffer being recorded. */
        VkCommandBuffer m_Commands = nullptr;

        /** Optional graph used by the graph-aware helpers to resolve resources. */
        const RenderGraph* m_Graph = nullptr;

        /** Per bind point (graphics, compute) flag telling whether the global bindless set is already bound. */
        std::array<bool, 2> m_BindlessBound = {false, false};

        /** Workgroup size of the bound compute pipeline (DispatchThreads). */
        std::array<std::uint32_t, 3> m_ThreadGroupSize = {1U, 1U, 1U};

        /** Reflected push block size of the bound pipeline (0 = unknown or none). */
        std::uint32_t m_PushSize = 0U;

    public:
        /** Constructor; Graph is optional and only required by the graph-aware helpers. */
        CommandList(Context& Ctx, VkCommandBuffer Commands, const RenderGraph* Graph = nullptr);

        /** Raw Vulkan command buffer, always available for direct vkCmd* calls. */
        [[nodiscard]] VkCommandBuffer Handle() const noexcept { return m_Commands; }

        /** Context this command list records into. */
        [[nodiscard]] Context&        GetContext() const noexcept { return *m_Context; }

        /** Binds the pipeline and, once per bind point, the global bindless descriptor set. */
        void BindPipeline(const Pipeline& Target);

        /** Writes up to luvk::PushConstantSize bytes, visible to every shader stage. */
        void PushConstants(const void* Data, std::uint32_t Size, std::uint32_t Offset = 0U);

        template <typename T>
            requires std::is_trivially_copyable_v<T>

        /** Convenience overload writing any trivially copyable value at the 4-byte aligned byte offset. */
        void PushConstants(const T& Value, const std::uint32_t Offset = 0U)
        {
            static_assert(sizeof(T) <= PushConstantSize, "Push constants are limited to luvk::PushConstantSize bytes");
            PushConstants(&Value, static_cast<std::uint32_t>(sizeof(T)), Offset);
        }

        /** Sets the viewport to (0, 0)-(Extent.Width, Extent.Height) with depth range [0, 1]. */
        void SetViewport(Extent2D Extent);

        /** Sets a single viewport at first viewport slot 0. */
        void SetViewport(const Viewport& Viewport);

        /** Sets a single scissor rectangle at first scissor slot 0. */
        void SetScissor(const Rect2D& Scissor);

        /** Sets the depth bias of a pipeline created with DepthBiasDesc::Dynamic (a non-zero Clamp needs depthBiasClamp). */
        void SetDepthBias(float ConstantFactor, float SlopeFactor, float Clamp = 0.F);

        /** Sets the stencil reference of a pipeline created with StencilDesc::DynamicReference. */
        void SetStencilReference(std::uint32_t Reference, StencilFace Faces = StencilFace::FrontAndBack);

        /** Non-indexed draw of VertexCount vertices and InstanceCount instances. */
        void Draw(std::uint32_t VertexCount, std::uint32_t InstanceCount = 1U, std::uint32_t FirstVertex = 0U, std::uint32_t FirstInstance = 0U);

        /** Indexed draw of IndexCount indices using the currently bound index buffer. */
        void DrawIndexed(std::uint32_t IndexCount, std::uint32_t InstanceCount = 1U, std::uint32_t FirstIndex = 0U, std::int32_t VertexOffset = 0, std::uint32_t FirstInstance = 0U);

        /** Mesh shader dispatch (requires the mesh shader module / VK_EXT_mesh_shader). */
        void DrawMeshTasks(std::uint32_t GroupsX, std::uint32_t GroupsY = 1U, std::uint32_t GroupsZ = 1U);

        /** Compute dispatch of GroupsX x GroupsY x GroupsZ workgroups. */
        void Dispatch(std::uint32_t GroupsX, std::uint32_t GroupsY = 1U, std::uint32_t GroupsZ = 1U);

        /** Compute dispatch covering X x Y x Z threads: the group count is derived from the bound pipeline's [numthreads]. */
        void DispatchThreads(std::uint32_t X, std::uint32_t Y = 1U, std::uint32_t Z = 1U);

        /**
         * GPU-driven work: arguments are read from graph buffers declared with Access::IndirectBuffer (Arguments and
         * Count). The *Count variants draw up to MaxDrawCount records, the actual number being the uint32 at
         * CountOffset in Count; they require drawIndirectCount (enabled when the device supports it).
         */

        /** DrawCount DrawIndirectArgs records read from Arguments at Offset. */
        void DrawIndirect(BufferHandle Arguments, std::uint32_t DrawCount, DeviceSize Offset = 0U, std::uint32_t Stride = sizeof(DrawIndirectArgs));

        /** DrawCount DrawIndexedIndirectArgs records read from Arguments at Offset (uses the bound index buffer). */
        void DrawIndexedIndirect(BufferHandle Arguments, std::uint32_t DrawCount, DeviceSize Offset = 0U, std::uint32_t Stride = sizeof(DrawIndexedIndirectArgs));

        /** Up to MaxDrawCount DrawIndirectArgs records, the count being read from Count. */
        void DrawIndirectCount(BufferHandle Arguments, BufferHandle Count, std::uint32_t MaxDrawCount, DeviceSize Offset = 0U, DeviceSize CountOffset = 0U,
                               std::uint32_t Stride = sizeof(DrawIndirectArgs));

        /** Up to MaxDrawCount DrawIndexedIndirectArgs records, the count being read from Count. */
        void DrawIndexedIndirectCount(BufferHandle Arguments, BufferHandle Count, std::uint32_t MaxDrawCount, DeviceSize Offset = 0U,
                                      DeviceSize CountOffset = 0U, std::uint32_t Stride = sizeof(DrawIndexedIndirectArgs));

        /** DrawCount DrawMeshTasksIndirectArgs records read from Arguments (requires the mesh shader module). */
        void DrawMeshTasksIndirect(BufferHandle Arguments, std::uint32_t DrawCount, DeviceSize Offset = 0U,
                                   std::uint32_t Stride = sizeof(DrawMeshTasksIndirectArgs));

        /** Up to MaxDrawCount mesh task records, the count being read from Count (requires the mesh shader module and drawIndirectCount). */
        void DrawMeshTasksIndirectCount(BufferHandle Arguments, BufferHandle Count, std::uint32_t MaxDrawCount, DeviceSize Offset = 0U,
                                        DeviceSize CountOffset = 0U, std::uint32_t Stride = sizeof(DrawMeshTasksIndirectArgs));

        /** One DispatchIndirectArgs record read from Arguments at Offset. */
        void DispatchIndirect(BufferHandle Arguments, DeviceSize Offset = 0U);

        /** Binds an index buffer at the given byte offset. */
        void BindIndexBuffer(const Buffer& Indices, IndexType Type, DeviceSize Offset = 0U);

        /** Binds per-frame index data. */
        void BindIndexBuffer(const FrameAllocation& Indices, IndexType Type);

        /** Binds a vertex buffer at the given binding slot and byte offset. */
        void BindVertexBuffer(std::uint32_t Binding, const Buffer& Vertices, DeviceSize Offset = 0U);

        /** Binds per-frame vertex data at the given binding slot. */
        void BindVertexBuffer(std::uint32_t Binding, const FrameAllocation& Vertices);

        /**
         * Graph-aware helpers: resources must be declared in the pass with the matching Access
         * (e.g. Access::TransferDst for FillBuffer). Texture helpers act on the subresources the handle selects
         * (TextureHandle::Mip / Layer); a whole handle means every mip and layer for ClearColor, the first mip and
         * layer for copies and blits.
         */

        /** Clears the selected mips and layers of the graph texture with Color. */
        void ClearColor(TextureHandle Target, const ClearColorValue& Color);

        /** Clears the selected mips and layers of the graph texture with a float color. */
        void ClearColor(TextureHandle Target, float Red, float Green, float Blue, float Alpha);

        /** Fills Size bytes (WholeSize: up to the end) of the graph buffer from Offset with the 32-bit Value (both multiples of 4). */
        void FillBuffer(BufferHandle Target, std::uint32_t Value, DeviceSize Offset = 0U, DeviceSize Size = WholeSize);

        /** Copies Size bytes (WholeSize: the smaller of both buffer sizes) from Source to Destination. */
        void CopyBuffer(BufferHandle Source, BufferHandle Destination, DeviceSize Size = WholeSize);

        /** Copies every region (bounds-checked) from Source to Destination in one command, e.g. scattering ranges into a pool. */
        void CopyBuffer(BufferHandle Source, BufferHandle Destination, std::span<const BufferCopy> Regions);

        /**
         * Copies a frame allocation (written by the CPU this frame) into Destination at DestinationOffset. Only the
         * destination is declared to the graph (Access::TransferDst): host writes are visible at submission.
         */
        void CopyBuffer(const FrameAllocation& Source, BufferHandle Destination, DeviceSize DestinationOffset = 0U);

        /** Copies one mip level / layer of Source into Destination with tightly packed rows. */
        void CopyTextureToBuffer(TextureHandle Source, BufferHandle Destination);

        /** Blits one mip level / layer of Source onto one mip level / layer of Destination using the given filter. */
        void BlitTexture(TextureHandle Source, TextureHandle Destination, luvk::Filter Filter = luvk::Filter::Linear);

        /** Resolved Vulkan objects of a graph texture or subresource (throws for invalid handles). */
        [[nodiscard]] GraphTexture ResolveTexture(TextureHandle Handle) const;

        /** Resolved Vulkan objects of a graph buffer (throws for invalid handles). */
        [[nodiscard]] const GraphBuffer&  ResolveBuffer(BufferHandle Handle) const;

        /** Opens a debug utils label named Name (no-op when debug utils is disabled). */
        void BeginLabel(std::string_view Name, std::array<float, 4> Color = {1.F, 1.F, 1.F, 1.F}) const;

        /** Closes the current debug utils label (no-op when debug utils is disabled). */
        void EndLabel() const;

    private:
        /** Graph used for handle resolution; throws when the list was created without one. */
        [[nodiscard]] const RenderGraph& Graph() const;

        /** Throws when VK_EXT_mesh_shader is not enabled. */
        void RequireMeshShaders() const;

        /** Throws when the drawIndirectCount feature is not enabled. */
        void RequireIndirectCount() const;
    };
}
