/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Types/Mesh.hpp"
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include "luvk/Types/Material.hpp"

using namespace luvk;

Mesh::Mesh(const std::shared_ptr<Device>& Device, const std::shared_ptr<Memory>& Memory)
    : m_Device(Device),
      m_Memory(Memory) {}

void Mesh::UploadVertices(const std::span<const std::byte> Data, const std::uint32_t VertexCount, const std::uint32_t FrameIndex)
{
    EnsureCapacityAndUpload(m_VertexBuffers.at(FrameIndex), Data, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, "Mesh VTX");

    m_VertexCount = VertexCount;
}

void Mesh::UploadIndices(const std::span<const std::uint16_t> Data, const std::uint32_t FrameIndex)
{
    m_IndexType = VK_INDEX_TYPE_UINT16;

    EnsureCapacityAndUpload(m_IndexBuffers.at(FrameIndex),
                            std::as_bytes(Data),
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            "Mesh IDX");

    m_IndexCount = static_cast<std::uint32_t>(std::size(Data));
}

void Mesh::UploadIndices(const std::span<const std::uint32_t> Data, const std::uint32_t FrameIndex)
{
    m_IndexType = VK_INDEX_TYPE_UINT32;

    EnsureCapacityAndUpload(m_IndexBuffers.at(FrameIndex),
                            std::as_bytes(Data),
                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                            "Mesh IDX");

    m_IndexCount = static_cast<std::uint32_t>(std::size(Data));
}

void Mesh::UpdateInstances(const std::span<const std::byte> Data, const std::uint32_t Count, const std::uint32_t FrameIndex)
{
    EnsureCapacityAndUpload(m_InstanceBuffers.at(FrameIndex),
                            Data,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                            "Instance Data");

    m_InstanceCount = Count;
}

void Mesh::UpdateUniformBuffer(const std::span<const std::byte> Data, const std::uint32_t FrameIndex)
{
    EnsureCapacityAndUpload(m_UniformBuffers.at(FrameIndex), Data, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, "Mesh UBO");
}

void Mesh::SetDispatchCount(const std::uint32_t X, const std::uint32_t Y, const std::uint32_t Z)
{
    m_DispatchX = X;
    m_DispatchY = Y;
    m_DispatchZ = Z;
}

void Mesh::SetPushConstantData(const std::span<const std::byte> Data)
{
    m_PushConstantData.assign(std::begin(Data), std::end(Data));
}

void Mesh::EnsureCapacityAndUpload(std::shared_ptr<Buffer>&         BufferObj,
                                   const std::span<const std::byte> Data,
                                   const VkBufferUsageFlags         Usage,
                                   const std::string_view           Name)
{
    if (const std::size_t RequiredSize = Data.size_bytes();
        !BufferObj || BufferObj->GetSize() < RequiredSize)
    {
        const std::size_t CurrentSize = BufferObj ? BufferObj->GetSize() : 0;

        const std::size_t NewSize = std::max(RequiredSize, CurrentSize * 2);

        if (!BufferObj)
        {
            BufferObj = std::make_shared<Buffer>(m_Device, m_Memory);
        }

        BufferObj->RecreateBuffer({.Size        = NewSize,
                                   .Usage       = Usage,
                                   .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                                   .Name        = std::string(Name)});
    }

    BufferObj->Upload(Data);
}

void Mesh::Tick(const float DeltaTime) {}

void Mesh::Render(const VkCommandBuffer CommandBuffer, const std::uint32_t CurrentFrame) const
{
    if (!m_Material)
    {
        return;
    }

    if (CurrentFrame < Constants::ImageCount && m_UniformBuffers.at(CurrentFrame))
    {
        m_Material->SetUniformBuffer(m_UniformBuffers.at(CurrentFrame), 0);
    }

    m_Material->Bind(CommandBuffer);
    const auto Pipeline     = m_Material->GetPipeline();
    const auto PipelineType = Pipeline->GetType();

    if (!std::empty(m_PushConstantData))
    {
        VkShaderStageFlags Stages = 0;
        if (const auto& Ranges = Pipeline->GetPushConstants();
            !std::empty(Ranges))
        {
            for (const auto& Range : Ranges)
            {
                Stages |= Range.stageFlags;
            }
        }
        else
        {
            Stages = PipelineType == Pipeline::Type::Compute
                         ? VK_SHADER_STAGE_COMPUTE_BIT
                         : PipelineType == Pipeline::Type::Mesh
                               ? VK_SHADER_STAGE_MESH_BIT_EXT
                               : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        vkCmdPushConstants(CommandBuffer,
                           Pipeline->GetPipelineLayout(),
                           Stages,
                           0,
                           static_cast<std::uint32_t>(std::size(m_PushConstantData)),
                           std::data(m_PushConstantData));
    }

    if (PipelineType == Pipeline::Type::Compute)
    {
        vkCmdDispatch(CommandBuffer, m_DispatchX, m_DispatchY, m_DispatchZ);
        return;
    }

    if (PipelineType == Pipeline::Type::Mesh)
    {
        if (vkCmdDrawMeshTasksEXT)
        {
            vkCmdDrawMeshTasksEXT(CommandBuffer, m_DispatchX, m_DispatchY, m_DispatchZ);
        }
        return;
    }

    std::array<VkBuffer, 2>     VtxBuffers{};
    std::array<VkDeviceSize, 2> Offsets{};
    std::uint32_t               BufferCount = 0;

    if (CurrentFrame < Constants::ImageCount)
    {
        if (m_VertexBuffers.at(CurrentFrame))
        {
            VtxBuffers[BufferCount] = m_VertexBuffers.at(CurrentFrame)->GetHandle();
            Offsets[BufferCount]    = 0;
            BufferCount++;
        }

        if (m_InstanceBuffers.at(CurrentFrame))
        {
            VtxBuffers[BufferCount] = m_InstanceBuffers.at(CurrentFrame)->GetHandle();
            Offsets[BufferCount]    = 0;
            BufferCount++;
        }
    }

    if (BufferCount > 0)
    {
        vkCmdBindVertexBuffers(CommandBuffer, 0, BufferCount, std::data(VtxBuffers), std::data(Offsets));
    }

    if (CurrentFrame < Constants::ImageCount && m_IndexBuffers.at(CurrentFrame))
    {
        vkCmdBindIndexBuffer(CommandBuffer, m_IndexBuffers.at(CurrentFrame)->GetHandle(), 0, m_IndexType);
        vkCmdDrawIndexed(CommandBuffer, m_IndexCount, std::max(1U, m_InstanceCount), 0, 0, 0);
    }
    else
    {
        vkCmdDraw(CommandBuffer, m_VertexCount, std::max(1U, m_InstanceCount), 0, 0);
    }
}
