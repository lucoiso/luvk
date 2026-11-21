// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Render/CommandList.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Context.hpp"
#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Vulkan.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/Pipeline.hpp"

luvk::CommandList::CommandList(Context& Ctx, const VkCommandBuffer Commands, const RenderGraph* const Graph)
    : m_Context(&Ctx),
      m_Commands(Commands),
      m_Graph(Graph) {}

void luvk::CommandList::BindPipeline(const Pipeline& Target)
{
    if (!Target)
    {
        throw std::logic_error("[luvk] BindPipeline called with an empty pipeline.");
    }

    const VkPipelineBindPoint BindPoint = Target.IsCompute() ? VK_PIPELINE_BIND_POINT_COMPUTE : VK_PIPELINE_BIND_POINT_GRAPHICS;
    vkCmdBindPipeline(m_Commands, BindPoint, Target.Handle());
    m_ThreadGroupSize = Target.ThreadGroupSize();
    m_PushSize        = Target.PushSize();

    const std::size_t Slot = Target.IsCompute() ? 1U : 0U;
    if (!m_BindlessBound[Slot])
    {
        const VkDescriptorSet Set = m_Context->Bindless().Set();
        vkCmdBindDescriptorSets(m_Commands, BindPoint, m_Context->Bindless().PipelineLayout(), 0U, 1U, &Set, 0U, nullptr);
        m_BindlessBound[Slot] = true;
    }
}

void luvk::CommandList::PushConstants(const void* const Data, const std::uint32_t Size, const std::uint32_t Offset)
{
    if (Size == 0U || Offset % 4U != 0U || Size % 4U != 0U || Offset > PushConstantSize || Size > PushConstantSize - Offset)
    {
        throw std::out_of_range("[luvk] Push constants must be 4-byte aligned and fit in luvk::PushConstantSize bytes.");
    }

#ifndef NDEBUG
    // A push struct smaller than the block the shader reads leaves stale bytes in the tail.
    if (Offset == 0U && m_PushSize != 0U && Size < m_PushSize)
    {
        throw std::invalid_argument("[luvk] PushConstants: " + std::to_string(Size) + " bytes pushed, the bound pipeline reads " + std::to_string(m_PushSize) + ".");
    }
#endif

    vkCmdPushConstants(m_Commands, m_Context->Bindless().PipelineLayout(), VK_SHADER_STAGE_ALL, Offset, Size, Data);
}

void luvk::CommandList::SetViewport(const luvk::Extent2D Extent)
{
    SetViewport(luvk::Viewport{0.F, 0.F, static_cast<float>(Extent.Width), static_cast<float>(Extent.Height), 0.F, 1.F});
}

void luvk::CommandList::SetViewport(const luvk::Viewport& Viewport)
{
    const VkViewport Value = ToVk(Viewport);
    vkCmdSetViewport(m_Commands, 0U, 1U, &Value);
}

void luvk::CommandList::SetScissor(const luvk::Rect2D& Scissor)
{
    const VkRect2D Value = ToVk(Scissor);
    vkCmdSetScissor(m_Commands, 0U, 1U, &Value);
}

void luvk::CommandList::SetDepthBias(const float ConstantFactor, const float SlopeFactor, const float Clamp)
{
    if (Clamp != 0.F && m_Context->EnabledFeatures().Core().depthBiasClamp == VK_FALSE)
    {
        throw std::logic_error("[luvk] A depth bias clamp requires the depthBiasClamp device feature.");
    }

    vkCmdSetDepthBias(m_Commands, ConstantFactor, Clamp, SlopeFactor);
}

void luvk::CommandList::SetStencilReference(const std::uint32_t Reference, const StencilFace Faces)
{
    vkCmdSetStencilReference(m_Commands, ToVk(Faces), Reference);
}

void luvk::CommandList::Draw(const std::uint32_t VertexCount, const std::uint32_t InstanceCount, const std::uint32_t FirstVertex, const std::uint32_t FirstInstance)
{
    vkCmdDraw(m_Commands, VertexCount, InstanceCount, FirstVertex, FirstInstance);
}

void luvk::CommandList::DrawIndexed(const std::uint32_t IndexCount,
                                    const std::uint32_t InstanceCount,
                                    const std::uint32_t FirstIndex,
                                    const std::int32_t  VertexOffset,
                                    const std::uint32_t FirstInstance)
{
    vkCmdDrawIndexed(m_Commands, IndexCount, InstanceCount, FirstIndex, VertexOffset, FirstInstance);
}

void luvk::CommandList::DrawMeshTasks(const std::uint32_t GroupsX, const std::uint32_t GroupsY, const std::uint32_t GroupsZ)
{
    RequireMeshShaders();
    vkCmdDrawMeshTasksEXT(m_Commands, GroupsX, GroupsY, GroupsZ);
}

void luvk::CommandList::Dispatch(const std::uint32_t GroupsX, const std::uint32_t GroupsY, const std::uint32_t GroupsZ)
{
    vkCmdDispatch(m_Commands, GroupsX, GroupsY, GroupsZ);
}

void luvk::CommandList::DispatchThreads(const std::uint32_t X, const std::uint32_t Y, const std::uint32_t Z)
{
    const auto Groups = [](const std::uint32_t Threads, const std::uint32_t Size) { return (Threads + Size - 1U) / Size; };
    vkCmdDispatch(m_Commands, Groups(X, m_ThreadGroupSize[0]), Groups(Y, m_ThreadGroupSize[1]), Groups(Z, m_ThreadGroupSize[2]));
}

void luvk::CommandList::DrawIndirect(const BufferHandle Arguments, const std::uint32_t DrawCount, const VkDeviceSize Offset, const std::uint32_t Stride)
{
    vkCmdDrawIndirect(m_Commands, ResolveBuffer(Arguments).Buffer, Offset, DrawCount, Stride);
}

void luvk::CommandList::DrawIndexedIndirect(const BufferHandle Arguments, const std::uint32_t DrawCount, const VkDeviceSize Offset, const std::uint32_t Stride)
{
    vkCmdDrawIndexedIndirect(m_Commands, ResolveBuffer(Arguments).Buffer, Offset, DrawCount, Stride);
}

void luvk::CommandList::DrawIndirectCount(const BufferHandle   Arguments,
                                          const BufferHandle   Count,
                                          const std::uint32_t  MaxDrawCount,
                                          const VkDeviceSize   Offset,
                                          const VkDeviceSize   CountOffset,
                                          const std::uint32_t  Stride)
{
    RequireIndirectCount();
    vkCmdDrawIndirectCount(m_Commands, ResolveBuffer(Arguments).Buffer, Offset, ResolveBuffer(Count).Buffer, CountOffset, MaxDrawCount, Stride);
}

void luvk::CommandList::DrawIndexedIndirectCount(const BufferHandle   Arguments,
                                                 const BufferHandle   Count,
                                                 const std::uint32_t  MaxDrawCount,
                                                 const VkDeviceSize   Offset,
                                                 const VkDeviceSize   CountOffset,
                                                 const std::uint32_t  Stride)
{
    RequireIndirectCount();
    vkCmdDrawIndexedIndirectCount(m_Commands, ResolveBuffer(Arguments).Buffer, Offset, ResolveBuffer(Count).Buffer, CountOffset, MaxDrawCount, Stride);
}

void luvk::CommandList::DrawMeshTasksIndirect(const BufferHandle Arguments, const std::uint32_t DrawCount, const VkDeviceSize Offset, const std::uint32_t Stride)
{
    RequireMeshShaders();
    vkCmdDrawMeshTasksIndirectEXT(m_Commands, ResolveBuffer(Arguments).Buffer, Offset, DrawCount, Stride);
}

void luvk::CommandList::DrawMeshTasksIndirectCount(const BufferHandle   Arguments,
                                                   const BufferHandle   Count,
                                                   const std::uint32_t  MaxDrawCount,
                                                   const VkDeviceSize   Offset,
                                                   const VkDeviceSize   CountOffset,
                                                   const std::uint32_t  Stride)
{
    RequireMeshShaders();
    RequireIndirectCount();
    vkCmdDrawMeshTasksIndirectCountEXT(m_Commands, ResolveBuffer(Arguments).Buffer, Offset, ResolveBuffer(Count).Buffer, CountOffset, MaxDrawCount, Stride);
}

void luvk::CommandList::DispatchIndirect(const BufferHandle Arguments, const VkDeviceSize Offset)
{
    vkCmdDispatchIndirect(m_Commands, ResolveBuffer(Arguments).Buffer, Offset);
}

void luvk::CommandList::RequireMeshShaders() const
{
    if (vkCmdDrawMeshTasksEXT == nullptr)
    {
        throw std::logic_error("[luvk] Mesh task draws require luvk::MeshShaderModule (VK_EXT_mesh_shader).");
    }
}

void luvk::CommandList::RequireIndirectCount() const
{
    if (m_Context->EnabledFeatures().Vulkan12().drawIndirectCount == VK_FALSE)
    {
        throw std::logic_error("[luvk] Indirect count draws require the drawIndirectCount feature (not supported by this device).");
    }
}

void luvk::CommandList::BindIndexBuffer(const luvk::Buffer& Indices, const IndexType Type, const VkDeviceSize Offset)
{
    vkCmdBindIndexBuffer(m_Commands, Indices.Handle(), Offset, ToVk(Type));
}

void luvk::CommandList::BindIndexBuffer(const FrameAllocation& Indices, const IndexType Type)
{
    vkCmdBindIndexBuffer(m_Commands, Indices.Buffer, Indices.Offset, ToVk(Type));
}

void luvk::CommandList::BindVertexBuffer(const std::uint32_t Binding, const luvk::Buffer& Vertices, const VkDeviceSize Offset)
{
    const VkBuffer Handle = Vertices.Handle();
    vkCmdBindVertexBuffers(m_Commands, Binding, 1U, &Handle, &Offset);
}

void luvk::CommandList::BindVertexBuffer(const std::uint32_t Binding, const FrameAllocation& Vertices)
{
    vkCmdBindVertexBuffers(m_Commands, Binding, 1U, &Vertices.Buffer, &Vertices.Offset);
}

void luvk::CommandList::ClearColor(const TextureHandle Target, const ClearColorValue& Color)
{
    const GraphTexture            Texture = ResolveTexture(Target);
    const VkImageSubresourceRange Range   = {Texture.Aspect, Texture.BaseMipLevel, Texture.MipLevels, Texture.BaseArrayLayer, Texture.ArrayLayers};
    const VkClearColorValue       Value   = ToVk(Color);
    vkCmdClearColorImage(m_Commands, Texture.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &Value, 1U, &Range);
}

void luvk::CommandList::ClearColor(const TextureHandle Target, const float Red, const float Green, const float Blue, const float Alpha)
{
    ClearColor(Target, ClearColorValue{.Float = {Red, Green, Blue, Alpha}});
}

void luvk::CommandList::FillBuffer(const BufferHandle Target, const std::uint32_t Value, const VkDeviceSize Offset, const VkDeviceSize Size)
{
    const GraphBuffer& To = ResolveBuffer(Target);

    if (Offset % 4U != 0U || (Size != VK_WHOLE_SIZE && Size % 4U != 0U))
    {
        throw std::invalid_argument("[luvk] FillBuffer offset and size must be multiples of 4.");
    }

    if (Offset >= To.Size || (Size != VK_WHOLE_SIZE && Size > To.Size - Offset))
    {
        throw std::out_of_range("[luvk] FillBuffer range exceeds the buffer.");
    }

    vkCmdFillBuffer(m_Commands, To.Buffer, Offset, Size, Value);
}

void luvk::CommandList::CopyBuffer(const BufferHandle Source, const BufferHandle Destination, const std::span<const BufferCopy> Regions)
{
    if (std::empty(Regions))
    {
        return;
    }

    const GraphBuffer& From = ResolveBuffer(Source);
    const GraphBuffer& To   = ResolveBuffer(Destination);

    std::vector<VkBufferCopy2> Copies;
    Copies.reserve(std::size(Regions));

    for (const BufferCopy& Region : Regions)
    {
        if (Region.Size == 0U || Region.SourceOffset > From.Size || Region.Size > From.Size - Region.SourceOffset ||
            Region.DestinationOffset > To.Size || Region.Size > To.Size - Region.DestinationOffset)
        {
            throw std::out_of_range("[luvk] CopyBuffer region is empty or exceeds one of the buffers.");
        }

        Copies.push_back(VkBufferCopy2{.sType     = VK_STRUCTURE_TYPE_BUFFER_COPY_2,
                                       .srcOffset = Region.SourceOffset,
                                       .dstOffset = Region.DestinationOffset,
                                       .size      = Region.Size});
    }

    const VkCopyBufferInfo2 Info{.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                                 .srcBuffer   = From.Buffer,
                                 .dstBuffer   = To.Buffer,
                                 .regionCount = static_cast<std::uint32_t>(std::size(Copies)),
                                 .pRegions    = std::data(Copies)};
    vkCmdCopyBuffer2(m_Commands, &Info);
}

void luvk::CommandList::CopyBuffer(const FrameAllocation& Source, const BufferHandle Destination, const VkDeviceSize DestinationOffset)
{
    if (!Source)
    {
        throw std::invalid_argument("[luvk] CopyBuffer from an empty FrameAllocation.");
    }

    const GraphBuffer& To = ResolveBuffer(Destination);
    if (DestinationOffset > To.Size || Source.Size > To.Size - DestinationOffset)
    {
        throw std::out_of_range("[luvk] CopyBuffer: the frame allocation does not fit in the destination.");
    }

    const VkBufferCopy2     Region{.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2, .srcOffset = Source.Offset, .dstOffset = DestinationOffset, .size = Source.Size};
    const VkCopyBufferInfo2 Info{.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                                 .srcBuffer   = Source.Buffer,
                                 .dstBuffer   = To.Buffer,
                                 .regionCount = 1U,
                                 .pRegions    = &Region};
    vkCmdCopyBuffer2(m_Commands, &Info);
}

void luvk::CommandList::CopyBuffer(const BufferHandle Source, const BufferHandle Destination, const VkDeviceSize Size)
{
    const GraphBuffer& From = ResolveBuffer(Source);
    const GraphBuffer& To   = ResolveBuffer(Destination);

    const VkDeviceSize Bytes = Size == VK_WHOLE_SIZE ? std::min(From.Size, To.Size) : Size;
    if (Bytes > From.Size || Bytes > To.Size)
    {
        throw std::out_of_range("[luvk] CopyBuffer size exceeds one of the buffers.");
    }

    const VkBufferCopy2     Region{.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2, .size = Bytes};
    const VkCopyBufferInfo2 Info{.sType       = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2,
                                 .srcBuffer   = From.Buffer,
                                 .dstBuffer   = To.Buffer,
                                 .regionCount = 1U,
                                 .pRegions    = &Region};
    vkCmdCopyBuffer2(m_Commands, &Info);
}

void luvk::CommandList::CopyTextureToBuffer(const TextureHandle Source, const BufferHandle Destination)
{
    const GraphTexture From = ResolveTexture(Source);
    const GraphBuffer& To   = ResolveBuffer(Destination);

    const VkDeviceSize Needed = FormatRegionSize(From.Format, From.Extent);
    if (Needed == 0U || Needed > To.Size)
    {
        throw std::out_of_range("[luvk] CopyTextureToBuffer: destination too small or unknown format.");
    }

    const VkImageAspectFlags Aspect = (From.Aspect & VK_IMAGE_ASPECT_DEPTH_BIT) != 0U ? VK_IMAGE_ASPECT_DEPTH_BIT : From.Aspect;

    const VkBufferImageCopy2 Region{.sType            = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2,
                                    .imageSubresource = {Aspect, From.BaseMipLevel, From.BaseArrayLayer, 1U},
                                    .imageExtent      = ToVk(From.Extent)};

    const VkCopyImageToBufferInfo2 Info{.sType          = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_BUFFER_INFO_2,
                                        .srcImage       = From.Image,
                                        .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        .dstBuffer      = To.Buffer,
                                        .regionCount    = 1U,
                                        .pRegions       = &Region};
    vkCmdCopyImageToBuffer2(m_Commands, &Info);
}

void luvk::CommandList::BlitTexture(const TextureHandle Source, const TextureHandle Destination, const luvk::Filter Filter)
{
    if (m_Graph != nullptr && m_Graph->Queue() == GraphQueue::AsyncCompute && m_Context->HasAsyncCompute())
    {
        throw std::logic_error("[luvk] BlitTexture requires a graphics queue: blits are not available in the async compute graph.");
    }

    const GraphTexture From = ResolveTexture(Source);
    const GraphTexture To   = ResolveTexture(Destination);

    const VkImageBlit2 Region{.sType          = VK_STRUCTURE_TYPE_IMAGE_BLIT_2,
                              .srcSubresource = {From.Aspect, From.BaseMipLevel, From.BaseArrayLayer, 1U},
                              .srcOffsets     = {{0, 0, 0}, {static_cast<std::int32_t>(From.Extent.Width), static_cast<std::int32_t>(From.Extent.Height), 1}},
                              .dstSubresource = {To.Aspect, To.BaseMipLevel, To.BaseArrayLayer, 1U},
                              .dstOffsets     = {{0, 0, 0}, {static_cast<std::int32_t>(To.Extent.Width), static_cast<std::int32_t>(To.Extent.Height), 1}}};

    const VkBlitImageInfo2 Info{.sType          = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2,
                                .srcImage       = From.Image,
                                .srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                .dstImage       = To.Image,
                                .dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                .regionCount    = 1U,
                                .pRegions       = &Region,
                                .filter         = ToVk(Filter)};
    vkCmdBlitImage2(m_Commands, &Info);
}

luvk::GraphTexture luvk::CommandList::ResolveTexture(const TextureHandle Handle) const
{
    return Graph().ResolveTexture(Handle);
}

const luvk::GraphBuffer& luvk::CommandList::ResolveBuffer(const BufferHandle Handle) const
{
    return Graph().ResolveBuffer(Handle);
}

void luvk::CommandList::BeginLabel(const std::string_view Name, const std::array<float, 4> Color) const
{
    if (!m_Context->IsDebugUtilsEnabled())
    {
        return;
    }

    const std::string    Copy(Name);
    VkDebugUtilsLabelEXT Label{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, .pLabelName = Copy.c_str()};
    std::ranges::copy(Color, std::begin(Label.color));
    vkCmdBeginDebugUtilsLabelEXT(m_Commands, &Label);
}

void luvk::CommandList::EndLabel() const
{
    if (m_Context->IsDebugUtilsEnabled())
    {
        vkCmdEndDebugUtilsLabelEXT(m_Commands);
    }
}

const luvk::RenderGraph& luvk::CommandList::Graph() const
{
    if (m_Graph == nullptr)
    {
        throw std::logic_error("[luvk] This CommandList is not associated with a RenderGraph.");
    }
    return *m_Graph;
}
