// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/MeshRegistry.hpp"
#include "luvk/Modules/Renderer.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"
#include <iterator>
#include "luvk/Resources/Image.hpp"
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Resources/Sampler.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include "luvk/Types/Texture.hpp"
#include "luvk/Types/Material.hpp"
#include <array>
#include <vector>

void luvk::MeshRegistry::Initialize(std::shared_ptr<Memory> const& MemoryModule)
{
    m_MemoryModule = MemoryModule;
}

std::size_t luvk::MeshRegistry::RegisterMesh(const std::span<std::byte const> Vertices,
                                             const std::span<std::byte const> Indices,
                                             const VkDescriptorSetLayout Layout,
                                             std::shared_ptr<DescriptorPool> const& Pool,
                                             std::shared_ptr<Image> const& TexImage,
                                             std::shared_ptr<Sampler> const& TexSampler,
                                             std::shared_ptr<Buffer> const& UniformBuffer,
                                             const std::span<InstanceInfo const> Instances,
                                             std::shared_ptr<Pipeline> const& PipelineModule,
                                             std::shared_ptr<Device> const& DeviceModule,
                                             const std::uint32_t TaskCount)
{
    constexpr VkBufferUsageFlags VertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    constexpr VkBufferUsageFlags IndexUsage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    MeshEntry Entry{};

    // Create vertex/index buffers only when data is provided
    if (!std::empty(Vertices))
    {
        Entry.VertexBuffer = std::make_shared<Buffer>();
        Entry.VertexBuffer->CreateBuffer(m_MemoryModule,
                                         {.Usage = VertexUsage, .Size = std::size(Vertices), .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
        Entry.VertexBuffer->Upload(Vertices);
    }

    if (!std::empty(Indices))
    {
        Entry.IndexBuffer = std::make_shared<Buffer>();
        Entry.IndexBuffer->CreateBuffer(m_MemoryModule,
                                        {.Usage = IndexUsage, .Size = std::size(Indices), .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
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

    if (PipelineModule && PipelineModule->GetType() == Pipeline::Type::Graphics)
    {
        Entry.IndexCount = static_cast<std::uint32_t>(std::size(Indices) / sizeof(std::uint16_t));
    }
    else
    {
        Entry.IndexCount = 0;
    }
    Entry.MaterialPtr->SetPipeline(PipelineModule);

    if (!std::empty(Instances))
    {
        CreateInstanceBuffer(Entry, Instances);
    }

    if (Pool && Layout != VK_NULL_HANDLE)
    {
        auto Descriptor = std::make_shared<DescriptorSet>();
        Descriptor->UseLayout(DeviceModule, Layout);
        Descriptor->Allocate(DeviceModule, Pool, m_MemoryModule);

        if (UniformBuffer)
        {
            Descriptor->UpdateBuffer(DeviceModule,
                                     UniformBuffer->GetHandle(),
                                     UniformBuffer->GetSize(),
                                     0,
                                     VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        }
        else if (TexImage)
        {
            const VkSampler SamplerHandle = TexSampler
                                                ? TexSampler->GetHandle()
                                                : VK_NULL_HANDLE;
            Descriptor->UpdateImage(DeviceModule,
                                    TexImage->GetView(),
                                    SamplerHandle,
                                    0,
                                    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        }

        Entry.MaterialPtr->SetDescriptor(std::move(Descriptor));
    }

    if (TexImage)
    {
        auto TexObj = std::make_shared<Texture>(TexImage, TexSampler);
        Entry.MaterialPtr->SetTexture(std::move(TexObj));
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

void luvk::MeshRegistry::SetPipeline(const std::size_t MeshIndex, std::shared_ptr<Pipeline> const& PipelineModule) const
{
    if (MeshIndex >= std::size(m_Meshes))
    {
        return;
    }

    if (m_Meshes[MeshIndex].MaterialPtr)
    {
        m_Meshes[MeshIndex].MaterialPtr->SetPipeline(PipelineModule);
    }
}

void luvk::MeshRegistry::CreateInstanceBuffer(MeshEntry& Entry, const std::span<InstanceInfo const> Instances) const
{
    constexpr VkBufferUsageFlags VertexUsage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    Entry.InstanceBuffer = std::make_shared<Buffer>();
    Entry.InstanceBuffer->CreateBuffer(m_MemoryModule,
                                       {.Usage = VertexUsage, .Size = sizeof(MeshInstance) * std::size(Instances), .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});

    std::vector<MeshInstance> InstanceData;
    InstanceData.reserve(std::size(Instances));

    for (auto const& Instance : Instances)
    {
        InstanceData.push_back({{Instance.XForm.Position[0], Instance.XForm.Position[1]}, Instance.XForm.Rotation[2], Instance.Color});
    }

    Entry.InstanceBuffer->Upload(std::as_bytes(std::span{InstanceData}));

    Entry.InstanceCount = static_cast<std::uint32_t>(std::size(Instances));
}

void luvk::MeshRegistry::UpdateInstances(const std::size_t MeshIndex, const std::span<InstanceInfo const> Instances)
{
    if (auto& MeshIt = m_Meshes.at(MeshIndex);
        !MeshIt.InstanceBuffer)
    {
        CreateInstanceBuffer(MeshIt, Instances);
    }
    else
    {
        if (VkDeviceSize const RequiredSize = sizeof(MeshInstance) * std::size(Instances);
            RequiredSize > MeshIt.InstanceBuffer->GetSize())
        {
            MeshIt.InstanceBuffer->RecreateBuffer({.Usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   .Size = RequiredSize,
                                                   .MemoryUsage = VMA_MEMORY_USAGE_CPU_TO_GPU});
        }

        std::vector<MeshInstance> Data;
        Data.reserve(std::size(Instances));

        for (auto const& Instance : Instances)
        {
            Data.push_back({{Instance.XForm.Position[0], Instance.XForm.Position[1]}, Instance.XForm.Rotation[2], Instance.Color});
        }

        MeshIt.InstanceBuffer->Upload(std::as_bytes(std::span{Data}));
        MeshIt.InstanceCount = static_cast<std::uint32_t>(std::size(Instances));
    }
}

void luvk::MeshRegistry::UpdateUniform(const std::size_t MeshIndex, const std::span<std::byte const> Data)
{
    auto& MeshIt = m_Meshes.at(MeshIndex);
    if (!MeshIt.UniformBuffer)
    {
        return;
    }

    MeshIt.UniformCache.assign(std::begin(Data), std::end(Data));
    MeshIt.UniformBuffer->Upload(Data);
}

void luvk::MeshRegistry::InitializeDependencies(std::shared_ptr<IRenderModule> const& /*MainRenderer*/)
{
    // Do nothing
}

void luvk::MeshRegistry::ClearResources()
{
    m_Meshes.clear();
}
