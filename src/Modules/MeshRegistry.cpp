// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/MeshRegistry.hpp"
#include <array>
#include <iterator>
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Resources/DescriptorSet.hpp"
#include "luvk/Resources/Image.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include "luvk/Resources/Sampler.hpp"
#include "luvk/Types/Material.hpp"
#include "luvk/Types/Texture.hpp"
#include "luvk/Types/Vector.hpp"

luvk::MeshRegistry::MeshRegistry(const std::shared_ptr<Device>& DeviceModule, const std::shared_ptr<Memory>& MemoryModule)
    : m_DeviceModule(DeviceModule),
      m_MemoryModule(MemoryModule) {}

std::size_t luvk::MeshRegistry::RegisterMesh(const std::span<const std::byte>& Vertices,
                                             const std::span<const std::byte>& Indices,
                                             const VkDescriptorSetLayout& Layout,
                                             const std::shared_ptr<DescriptorPool>& Pool,
                                             const std::shared_ptr<Image>& TexImage,
                                             const std::shared_ptr<Sampler>& TexSampler,
                                             const std::shared_ptr<Buffer>& UniformBuffer,
                                             const std::span<InstanceInfo>& Instances,
                                             const std::shared_ptr<Pipeline>& PipelineObj,
                                             const std::uint32_t TaskCount)
{
    constexpr VkBufferUsageFlags VertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    constexpr VkBufferUsageFlags IndexUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    MeshEntry Entry{};

    if (!std::empty(Vertices))
    {
        Entry.VertexBuffer = std::make_shared<Buffer>(m_DeviceModule, m_MemoryModule);
        Entry.VertexBuffer->CreateBuffer({.Name = "Mesh VTX",
                                          .Size = std::size(Vertices),
                                          .Usage = VertexUsage,
                                          .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});

        Entry.VertexBuffer->Upload(Vertices);
    }

    if (!std::empty(Indices))
    {
        Entry.IndexBuffer = std::make_shared<Buffer>(m_DeviceModule, m_MemoryModule);
        Entry.IndexBuffer->CreateBuffer({.Name = "Mesh IDX",
                                         .Size = std::size(Indices),
                                         .Usage = IndexUsage,
                                         .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});

        Entry.IndexBuffer->Upload(Indices);
    }

    Entry.MaterialPtr = std::make_shared<Material>();
    Entry.UniformBuffer = UniformBuffer;

    if (UniformBuffer)
    {
        Entry.UniformCache.resize(UniformBuffer->GetSize());
    }

    Entry.InstanceCount = static_cast<std::uint32_t>(std::size(Instances));

    Entry.DispatchX = TaskCount;
    Entry.DispatchY = 1;
    Entry.DispatchZ = 1;

    if (PipelineObj && PipelineObj->GetType() == Pipeline::Type::Graphics)
    {
        Entry.IndexCount = static_cast<std::uint32_t>(std::size(Indices) / sizeof(std::uint16_t));
    }
    else
    {
        Entry.IndexCount = 0;
    }
    Entry.MaterialPtr->SetPipeline(PipelineObj);

    if (!std::empty(Instances))
    {
        CreateInstanceBuffer(Entry, Instances);
    }

    if (Pool && Layout != VK_NULL_HANDLE)
    {
        const auto Descriptor = std::make_shared<DescriptorSet>(m_DeviceModule, Pool, m_MemoryModule);
        Descriptor->UseLayout(Layout);
        Descriptor->Allocate();

        if (UniformBuffer)
        {
            Descriptor->UpdateBuffer(UniformBuffer->GetHandle(),
                                     UniformBuffer->GetSize(),
                                     0,
                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        }
        else if (TexImage)
        {
            const VkSampler& SamplerHandle = TexSampler->GetHandle();
            Descriptor->UpdateImage(TexImage->GetView(),
                                    SamplerHandle,
                                    0,
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }

        Entry.MaterialPtr->SetDescriptor(Descriptor);
    }

    if (TexImage)
    {
        Entry.MaterialPtr->SetTexture(std::make_shared<Texture>(TexImage, TexSampler));
    }

    m_Meshes.push_back(std::move(Entry));
    return std::size(m_Meshes) - 1U;
}

bool luvk::MeshRegistry::RemoveMesh(const std::size_t MeshIndex)
{
    if (MeshIndex >= std::size(m_Meshes))
    {
        return false;
    }
    m_Meshes.erase(std::begin(m_Meshes) + static_cast<std::ptrdiff_t>(MeshIndex));
    return true;
}

void luvk::MeshRegistry::SetPipeline(const std::size_t MeshIndex, const std::shared_ptr<Pipeline>& PipelineObj) const
{
    if (MeshIndex >= std::size(m_Meshes))
    {
        return;
    }

    if (m_Meshes[MeshIndex].MaterialPtr)
    {
        m_Meshes[MeshIndex].MaterialPtr->SetPipeline(PipelineObj);
    }
}

void luvk::MeshRegistry::CreateInstanceBuffer(MeshEntry& Entry, const std::span<InstanceInfo>& Instances) const
{
    constexpr VkBufferUsageFlags VertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    Entry.InstanceBuffer = std::make_shared<Buffer>(m_DeviceModule, m_MemoryModule);
    Entry.InstanceBuffer->CreateBuffer({.Name = "Instance VTX",
                                        .Size = sizeof(MeshInstance) * std::size(Instances),
                                        .Usage = VertexUsage,
                                        .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});

    Vector<MeshInstance> InstanceData;
    InstanceData.reserve(std::size(Instances));

    for (const auto& Instance : Instances)
    {
        InstanceData.push_back({{Instance.XForm.Position[0], Instance.XForm.Position[1]}, Instance.XForm.Rotation[2], Instance.Color});
    }

    Entry.InstanceBuffer->Upload(std::as_bytes(std::span{InstanceData}));
    Entry.InstanceCount = static_cast<std::uint32_t>(std::size(Instances));
}

void luvk::MeshRegistry::UpdateInstances(const std::size_t MeshIndex, const std::span<InstanceInfo>& Instances)
{
    if (auto& MeshIt = m_Meshes.at(MeshIndex);
        !MeshIt.InstanceBuffer)
    {
        CreateInstanceBuffer(MeshIt, Instances);
    }
    else
    {
        if (const VkDeviceSize RequiredSize = sizeof(MeshInstance) * std::size(Instances);
            RequiredSize > MeshIt.InstanceBuffer->GetSize())
        {
            MeshIt.InstanceBuffer->RecreateBuffer({.Name = "Instance VTX",
                                                   .Size = RequiredSize,
                                                   .Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
        }

        Vector<MeshInstance> Data;
        Data.reserve(std::size(Instances));

        for (const auto& Instance : Instances)
        {
            Data.push_back({{Instance.XForm.Position[0], Instance.XForm.Position[1]}, Instance.XForm.Rotation[2], Instance.Color});
        }

        MeshIt.InstanceBuffer->Upload(std::as_bytes(std::span{Data}));
        MeshIt.InstanceCount = static_cast<std::uint32_t>(std::size(Instances));
    }
}

void luvk::MeshRegistry::UpdateUniform(const std::size_t MeshIndex, const std::span<const std::byte>& Data)
{
    auto& MeshIt = m_Meshes.at(MeshIndex);
    if (!MeshIt.UniformBuffer)
    {
        return;
    }

    MeshIt.UniformCache.assign(std::begin(Data), std::end(Data));
    MeshIt.UniformBuffer->Upload(Data);
}

void luvk::MeshRegistry::ClearResources()
{
    m_Meshes.clear();
}
