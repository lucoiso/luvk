// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Types/Mesh.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include "luvk/Types/Material.hpp"

using namespace luvk;

Mesh::Mesh(const std::shared_ptr<Device>& Device, const std::shared_ptr<Memory>& Memory)
    : m_Device(Device),
      m_Memory(Memory) {}

void Mesh::SetMaterial(const std::shared_ptr<Material>& MaterialObj)
{
    m_Material = MaterialObj;
}

const std::shared_ptr<Material>& Mesh::GetMaterial() const
{
    return m_Material;
}

void Mesh::UploadVertices(const std::span<const std::byte> Data, const std::uint32_t VertexCount)
{
    if (!m_VertexBuffer || m_VertexBuffer->GetSize() < Data.size_bytes())
    {
        m_VertexBuffer = std::make_shared<Buffer>(m_Device, m_Memory);
        m_VertexBuffer->CreateBuffer({.Name = "Mesh VTX",
                                      .Size = Data.size_bytes(),
                                      .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                      .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
    }
    m_VertexBuffer->Upload(Data);
    m_VertexCount = VertexCount;
}

void Mesh::UploadIndices(const std::span<const std::uint16_t> Data)
{
    const std::size_t Bytes = Data.size_bytes();
    if (!m_IndexBuffer || m_IndexBuffer->GetSize() < Bytes)
    {
        m_IndexBuffer = std::make_shared<Buffer>(m_Device, m_Memory);
        m_IndexBuffer->CreateBuffer({.Name = "Mesh IDX",
                                     .Size = Bytes,
                                     .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
    }
    m_IndexBuffer->Upload(std::as_bytes(Data));
    m_IndexCount = static_cast<std::uint32_t>(std::size(Data));
}

void Mesh::UploadIndices(const std::span<const std::uint32_t> Data)
{
    const std::size_t Bytes = Data.size_bytes();
    if (!m_IndexBuffer || m_IndexBuffer->GetSize() < Bytes)
    {
        m_IndexBuffer = std::make_shared<Buffer>(m_Device, m_Memory);
        m_IndexBuffer->CreateBuffer({.Name = "Mesh IDX",
                                     .Size = Bytes,
                                     .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                     .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
    }
    m_IndexBuffer->Upload(std::as_bytes(Data));
    m_IndexCount = static_cast<std::uint32_t>(std::size(Data));
}

void Mesh::SetDispatchCount(const std::uint32_t X, const std::uint32_t Y, const std::uint32_t Z)
{
    m_DispatchX = X;
    m_DispatchY = Y;
    m_DispatchZ = Z;
}

void Mesh::UpdateInstances(const std::span<const std::byte> Data, const std::uint32_t Count)
{
    if (!m_InstanceBuffer || m_InstanceBuffer->GetSize() < Data.size_bytes())
    {
        if (m_InstanceBuffer)
        {
            m_Device->WaitIdle();
        }

        m_InstanceBuffer = std::make_shared<Buffer>(m_Device, m_Memory);
        m_InstanceBuffer->CreateBuffer({.Name = "Instance Data",
                                        .Size = Data.size_bytes(),
                                        .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                        .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
    }
    m_InstanceBuffer->Upload(Data);
    m_InstanceCount = Count;
}

void Mesh::Draw(const VkCommandBuffer& CommandBuffer, const std::span<const std::byte> PushConstants) const
{
    if (!m_Material)
    {
        return;
    }

    m_Material->Bind(CommandBuffer);
    const auto& Pipeline     = m_Material->GetPipeline();
    const auto  PipelineType = Pipeline->GetType();

    if (!std::empty(PushConstants))
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
                           static_cast<std::uint32_t>(std::size(PushConstants)),
                           std::data(PushConstants));
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

    Vector<VkBuffer>     VtxBuffers;
    Vector<VkDeviceSize> Offsets;

    if (m_VertexBuffer)
    {
        VtxBuffers.push_back(m_VertexBuffer->GetHandle());
        Offsets.push_back(0);
    }
    if (m_InstanceBuffer)
    {
        VtxBuffers.push_back(m_InstanceBuffer->GetHandle());
        Offsets.push_back(0);
    }

    if (!std::empty(VtxBuffers))
    {
        vkCmdBindVertexBuffers(CommandBuffer, 0, static_cast<std::uint32_t>(std::size(VtxBuffers)), std::data(VtxBuffers), std::data(Offsets));
    }

    if (m_IndexBuffer)
    {
        vkCmdBindIndexBuffer(CommandBuffer, m_IndexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(CommandBuffer, m_IndexCount, std::max(1U, m_InstanceCount), 0, 0, 0);
    }
    else
    {
        vkCmdDraw(CommandBuffer, m_VertexCount, std::max(1U, m_InstanceCount), 0, 0);
    }
}
