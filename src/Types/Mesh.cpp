// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Types/Mesh.hpp"
#include "luvk/Types/Material.hpp"
#include <array>

inline void luvk::Mesh::Draw(const VkCommandBuffer CommandBuffer) const
{
    if (!m_Registry)
    {
        return;
    }

    auto const Meshes = m_Registry->GetMeshes();
    if (m_Index >= std::size(Meshes))
    {
        return;
    }

    auto const& Entry = Meshes[m_Index];
    if (Entry.InstanceCount == 0 && !Entry.UniformBuffer)
    {
        return;
    }

    std::array<VkBuffer, 2> Buffers{Entry.VertexBuffer->GetHandle(), VK_NULL_HANDLE};
    constexpr std::array<VkDeviceSize, 2> Offsets{0, 0};
    std::uint32_t BindingCount = 1U;

    if (Entry.InstanceBuffer)
    {
        Buffers.at(1) = Entry.InstanceBuffer->GetHandle();
        BindingCount = 2U;
    }

    vkCmdBindVertexBuffers(CommandBuffer, 0, BindingCount, std::data(Buffers), std::data(Offsets));
    vkCmdBindIndexBuffer(CommandBuffer, Entry.IndexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT16);

    auto const& Material = Entry.MaterialPtr;
    if (!Material || !Material->GetPipeline())
    {
        return;
    }

    vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Material->GetPipeline()->GetPipeline());

    if (auto const& Descriptor = Material->GetDescriptor())
    {
        vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Material->GetPipeline()->GetPipelineLayout(), 0, 1, &Descriptor->GetHandle(), 0, nullptr);
    }

    if (!std::empty(Entry.UniformCache))
    {
        vkCmdPushConstants(CommandBuffer,
                           Material->GetPipeline()->GetPipelineLayout(),
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           static_cast<std::uint32_t>(std::size(Entry.UniformCache)),
                           std::data(Entry.UniformCache));
    }

    const std::uint32_t DrawInstances = Entry.InstanceBuffer
                                            ? Entry.InstanceCount
                                            : 1U;
    vkCmdDrawIndexed(CommandBuffer, Entry.IndexCount, DrawInstances, 0, 0, 0);
}

inline void luvk::InstancedMesh::SetInstances(std::span<MeshRegistry::InstanceInfo const> Instances)
{
    m_Instances.assign(std::begin(Instances), std::end(Instances));
    if (m_Registry)
    {
        m_Registry->UpdateInstances(GetIndex(), m_Instances);
    }
}

inline void luvk::InstancedMesh::AddInstance(MeshRegistry::InstanceInfo const& Instance)
{
    m_Instances.push_back(Instance);
    if (m_Registry)
    {
        m_Registry->UpdateInstances(GetIndex(), m_Instances);
    }
}
