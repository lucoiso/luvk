// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

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
    auto& Buffer = m_VertexBuffers.at(FrameIndex);

    if (!Buffer || Buffer->GetSize() < Data.size_bytes())
    {
        Buffer = std::make_shared<luvk::Buffer>(m_Device, m_Memory);
        Buffer->CreateBuffer({.Size = Data.size_bytes(),
                              .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                              .Name = "Mesh VTX"});
    }
    Buffer->Upload(Data);
    m_VertexCount = VertexCount;
}

void Mesh::UploadIndices(const std::span<const std::uint16_t> Data, const std::uint32_t FrameIndex)
{
    m_IndexType = VK_INDEX_TYPE_UINT16;

    auto& Buffer = m_IndexBuffers.at(FrameIndex);

    if (const std::size_t Bytes = Data.size_bytes();
        !Buffer || Buffer->GetSize() < Bytes)
    {
        Buffer = std::make_shared<luvk::Buffer>(m_Device, m_Memory);
        Buffer->CreateBuffer({.Size = Bytes,
                              .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                              .Name = "Mesh IDX"});
    }
    Buffer->Upload(std::as_bytes(Data));
    m_IndexCount = static_cast<std::uint32_t>(std::size(Data));
}

void Mesh::UploadIndices(const std::span<const std::uint32_t> Data, const std::uint32_t FrameIndex)
{
    m_IndexType = VK_INDEX_TYPE_UINT32;

    auto& Buffer = m_IndexBuffers.at(FrameIndex);

    if (const std::size_t Bytes = Data.size_bytes();
        !Buffer || Buffer->GetSize() < Bytes)
    {
        Buffer = std::make_shared<luvk::Buffer>(m_Device, m_Memory);
        Buffer->CreateBuffer({.Size = Bytes,
                              .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                              .Name = "Mesh IDX"});
    }
    Buffer->Upload(std::as_bytes(Data));
    m_IndexCount = static_cast<std::uint32_t>(std::size(Data));
}

void Mesh::UpdateInstances(const std::span<const std::byte> Data, const std::uint32_t Count, const std::uint32_t FrameIndex)
{
    auto& Buffer = m_InstanceBuffers.at(FrameIndex);

    if (!Buffer || Buffer->GetSize() < Data.size_bytes())
    {
        Buffer = std::make_shared<luvk::Buffer>(m_Device, m_Memory);
        Buffer->CreateBuffer({.Size = Data.size_bytes(),
                              .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                              .Name = "Instance Data"});
    }
    Buffer->Upload(Data);
    m_InstanceCount = Count;
}

void Mesh::UpdateUniformBuffer(const std::span<const std::byte> Data, const std::uint32_t FrameIndex)
{
    auto& Buffer = m_UniformBuffers.at(FrameIndex);

    if (!Buffer || Buffer->GetSize() < Data.size_bytes())
    {
        Buffer = std::make_shared<luvk::Buffer>(m_Device, m_Memory);
        Buffer->CreateBuffer({.Size = Data.size_bytes(),
                              .Usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                              .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU,
                              .Name = "Mesh UBO"});
    }
    Buffer->Upload(Data);
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

    std::vector<VkBuffer>     VtxBuffers;
    std::vector<VkDeviceSize> Offsets;

    if (CurrentFrame < Constants::ImageCount)
    {
        if (m_VertexBuffers.at(CurrentFrame))
        {
            VtxBuffers.push_back(m_VertexBuffers.at(CurrentFrame)->GetHandle());
            Offsets.push_back(0);
        }

        if (m_InstanceBuffers.at(CurrentFrame))
        {
            VtxBuffers.push_back(m_InstanceBuffers.at(CurrentFrame)->GetHandle());
            Offsets.push_back(0);
        }
    }

    if (!std::empty(VtxBuffers))
    {
        vkCmdBindVertexBuffers(CommandBuffer, 0, static_cast<std::uint32_t>(std::size(VtxBuffers)), std::data(VtxBuffers), std::data(Offsets));
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
