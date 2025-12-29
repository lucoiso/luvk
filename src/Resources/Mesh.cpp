/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Mesh.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Resources/Material.hpp"

using namespace luvk;

void IMesh::SetMaterial(std::shared_ptr<Material> MaterialObj)
{
    m_Material = std::move(MaterialObj);
}

void IMesh::SetTransform(const Transform& Transform)
{
    m_Transform = Transform;
}

void Mesh::Render(const VkCommandBuffer CommandBuffer) const
{
    if (m_Material)
    {
        m_Material->Bind(CommandBuffer);
    }

    if (m_VertexBuffer)
    {
        const VkBuffer         BufferHandle = m_VertexBuffer->GetHandle();
        constexpr VkDeviceSize Offset       = 0;
        vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &BufferHandle, &Offset);
    }

    if (m_IndexBuffer)
    {
        vkCmdBindIndexBuffer(CommandBuffer, m_IndexBuffer->GetHandle(), 0, m_IndexType);
        vkCmdDrawIndexed(CommandBuffer, m_IndexCount, 1, 0, 0, 0);
    }
    else
    {
        vkCmdDraw(CommandBuffer, m_VertexCount, 1, 0, 0);
    }
}

void Mesh::UploadVertices(const std::span<const std::byte> Data, const std::uint32_t VertexCount)
{
    m_VertexCount  = VertexCount;
    m_VertexBuffer = std::make_shared<Buffer>(m_Device, m_Memory);
    m_VertexBuffer->CreateBuffer({.Size        = std::size(Data),
                                  .Usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                  .MemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
                                  .Name        = "StaticMeshVertex"});
    m_VertexBuffer->Upload(Data);
}

void Mesh::UploadIndices(const std::span<const std::uint16_t> Data)
{
    m_IndexType  = VK_INDEX_TYPE_UINT16;
    m_IndexCount = static_cast<std::uint32_t>(std::size(Data));

    m_IndexBuffer = std::make_shared<Buffer>(m_Device, m_Memory);
    m_IndexBuffer->CreateBuffer({.Size        = std::size(Data) * sizeof(std::uint16_t),
                                 .Usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 .MemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
                                 .Name        = "StaticMeshIndex"});
    m_IndexBuffer->Upload(std::as_bytes(Data));
}

void Mesh::UploadIndices(const std::span<const std::uint32_t> Data)
{
    m_IndexType  = VK_INDEX_TYPE_UINT32;
    m_IndexCount = static_cast<std::uint32_t>(std::size(Data));

    m_IndexBuffer = std::make_shared<Buffer>(m_Device, m_Memory);
    m_IndexBuffer->CreateBuffer({.Size        = std::size(Data) * sizeof(std::uint32_t),
                                 .Usage       = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                 .MemoryUsage = VMA_MEMORY_USAGE_GPU_ONLY,
                                 .Name        = "StaticMeshIndex"});
    m_IndexBuffer->Upload(std::as_bytes(Data));
}

void MeshShaderMesh::Render(const VkCommandBuffer CommandBuffer) const
{
    if (m_Material)
    {
        m_Material->Bind(CommandBuffer);
        vkCmdDrawMeshTasksEXT(CommandBuffer, m_DispatchX, m_DispatchY, m_DispatchZ);
    }
}
