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
        VkDeviceSize SourceOffset = 0U;

        /** Byte offset in the destination buffer. */
        VkDeviceSize DestinationOffset = 0U;

        /** Number of bytes to copy. */
        VkDeviceSize Size = 0U;
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
        VkCommandBuffer m_Commands = VK_NULL_HANDLE;

        /** Optional graph used by the graph-aware helpers to resolve resources. */
        const RenderGraph* m_Graph = nullptr;

        /** Per bind point (graphics, compute) flag telling whether the global bindless set is already bound. */
        std::array<bool, 2> m_BindlessBound = {false, false};

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

        /** Sets the viewport to (0, 0)-(Extent.width, Extent.height) with depth range [0, 1]. */
        void SetViewport(VkExtent2D Extent);

        /** Sets a single viewport at first viewport slot 0. */
        void SetViewport(const VkViewport& Viewport);

        /** Sets a single scissor rectangle at first scissor slot 0. */
        void SetScissor(const VkRect2D& Scissor);

        /** Sets the depth bias of a pipeline created with DepthBiasDesc::Dynamic (a non-zero Clamp needs depthBiasClamp). */
        void SetDepthBias(float ConstantFactor, float SlopeFactor, float Clamp = 0.F);

        /** Sets the stencil reference of a pipeline created with StencilDesc::DynamicReference. */
        void SetStencilReference(std::uint32_t Reference, VkStencilFaceFlags Faces = VK_STENCIL_FACE_FRONT_AND_BACK);

        /** Non-indexed draw of VertexCount vertices and InstanceCount instances. */
        void Draw(std::uint32_t VertexCount, std::uint32_t InstanceCount = 1U, std::uint32_t FirstVertex = 0U, std::uint32_t FirstInstance = 0U);

        /** Indexed draw of IndexCount indices using the currently bound index buffer. */
        void DrawIndexed(std::uint32_t IndexCount, std::uint32_t InstanceCount = 1U, std::uint32_t FirstIndex = 0U, std::int32_t VertexOffset = 0, std::uint32_t FirstInstance = 0U);

        /** Mesh shader dispatch (requires the mesh shader module / VK_EXT_mesh_shader). */
        void DrawMeshTasks(std::uint32_t GroupsX, std::uint32_t GroupsY = 1U, std::uint32_t GroupsZ = 1U);

        /** Compute dispatch of GroupsX x GroupsY x GroupsZ workgroups. */
        void Dispatch(std::uint32_t GroupsX, std::uint32_t GroupsY = 1U, std::uint32_t GroupsZ = 1U);

        /**
         * GPU-driven work: arguments are read from graph buffers declared with Access::IndirectBuffer (Arguments and
         * Count). The *Count variants draw up to MaxDrawCount records, the actual number being the uint32 at
         * CountOffset in Count; they require drawIndirectCount (enabled when the device supports it).
         */

        /** DrawCount VkDrawIndirectCommand records read from Arguments at Offset. */
        void DrawIndirect(BufferHandle Arguments, std::uint32_t DrawCount, VkDeviceSize Offset = 0U, std::uint32_t Stride = sizeof(VkDrawIndirectCommand));

        /** DrawCount VkDrawIndexedIndirectCommand records read from Arguments at Offset (uses the bound index buffer). */
        void DrawIndexedIndirect(BufferHandle Arguments, std::uint32_t DrawCount, VkDeviceSize Offset = 0U, std::uint32_t Stride = sizeof(VkDrawIndexedIndirectCommand));

        /** Up to MaxDrawCount VkDrawIndirectCommand records, the count being read from Count. */
        void DrawIndirectCount(BufferHandle Arguments, BufferHandle Count, std::uint32_t MaxDrawCount, VkDeviceSize Offset = 0U, VkDeviceSize CountOffset = 0U,
                               std::uint32_t Stride = sizeof(VkDrawIndirectCommand));

        /** Up to MaxDrawCount VkDrawIndexedIndirectCommand records, the count being read from Count. */
        void DrawIndexedIndirectCount(BufferHandle Arguments, BufferHandle Count, std::uint32_t MaxDrawCount, VkDeviceSize Offset = 0U,
                                      VkDeviceSize CountOffset = 0U, std::uint32_t Stride = sizeof(VkDrawIndexedIndirectCommand));

        /** DrawCount VkDrawMeshTasksIndirectCommandEXT records read from Arguments (requires the mesh shader module). */
        void DrawMeshTasksIndirect(BufferHandle Arguments, std::uint32_t DrawCount, VkDeviceSize Offset = 0U,
                                   std::uint32_t Stride = sizeof(VkDrawMeshTasksIndirectCommandEXT));

        /** Up to MaxDrawCount mesh task records, the count being read from Count (requires the mesh shader module and drawIndirectCount). */
        void DrawMeshTasksIndirectCount(BufferHandle Arguments, BufferHandle Count, std::uint32_t MaxDrawCount, VkDeviceSize Offset = 0U,
                                        VkDeviceSize CountOffset = 0U, std::uint32_t Stride = sizeof(VkDrawMeshTasksIndirectCommandEXT));

        /** One VkDispatchIndirectCommand read from Arguments at Offset. */
        void DispatchIndirect(BufferHandle Arguments, VkDeviceSize Offset = 0U);

        /** Binds an index buffer at the given byte offset. */
        void BindIndexBuffer(const Buffer& Indices, VkIndexType Type, VkDeviceSize Offset = 0U);

        /** Binds per-frame index data. */
        void BindIndexBuffer(const FrameAllocation& Indices, VkIndexType Type);

        /** Binds a vertex buffer at the given binding slot and byte offset. */
        void BindVertexBuffer(std::uint32_t Binding, const Buffer& Vertices, VkDeviceSize Offset = 0U);

        /** Binds per-frame vertex data at the given binding slot. */
        void BindVertexBuffer(std::uint32_t Binding, const FrameAllocation& Vertices);

        /**
         * Graph-aware helpers: resources must be declared in the pass with the matching Access
         * (e.g. Access::TransferDst for FillBuffer). Texture helpers act on the subresources the handle selects
         * (TextureHandle::Mip / Layer); a whole handle means every mip and layer for ClearColor, the first mip and
         * layer for copies and blits.
         */

        /** Clears the selected mips and layers of the graph texture with Color. */
        void ClearColor(TextureHandle Target, const VkClearColorValue& Color);

        /** Fills Size bytes (VK_WHOLE_SIZE: up to the end) of the graph buffer from Offset with the 32-bit Value (both multiples of 4). */
        void FillBuffer(BufferHandle Target, std::uint32_t Value, VkDeviceSize Offset = 0U, VkDeviceSize Size = VK_WHOLE_SIZE);

        /** Copies Size bytes (VK_WHOLE_SIZE: the smaller of both buffer sizes) from Source to Destination. */
        void CopyBuffer(BufferHandle Source, BufferHandle Destination, VkDeviceSize Size = VK_WHOLE_SIZE);

        /** Copies every region (bounds-checked) from Source to Destination in one command, e.g. scattering ranges into a pool. */
        void CopyBuffer(BufferHandle Source, BufferHandle Destination, std::span<const BufferCopy> Regions);

        /**
         * Copies a frame allocation (written by the CPU this frame) into Destination at DestinationOffset. Only the
         * destination is declared to the graph (Access::TransferDst): host writes are visible at submission.
         */
        void CopyBuffer(const FrameAllocation& Source, BufferHandle Destination, VkDeviceSize DestinationOffset = 0U);

        /** Copies one mip level / layer of Source into Destination with tightly packed rows. */
        void CopyTextureToBuffer(TextureHandle Source, BufferHandle Destination);

        /** Blits one mip level / layer of Source onto one mip level / layer of Destination using the given filter. */
        void BlitTexture(TextureHandle Source, TextureHandle Destination, VkFilter Filter = VK_FILTER_LINEAR);

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
