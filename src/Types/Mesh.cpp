// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Types/Mesh.hpp"
#include "luvk/Libraries/MeshDraw.hpp"

void luvk::Mesh::Draw(const VkCommandBuffer& CommandBuffer) const
{
    const auto Meshes = m_RegistryModule->GetMeshes();
    if (m_Index >= std::size(Meshes))
    {
        return;
    }

    const auto& Entry = Meshes[m_Index];
    RecordMeshCommands(CommandBuffer, Entry);
}

void luvk::InstancedMesh::SetInstances(const std::span<const MeshRegistry::InstanceInfo>& Instances)
{
    m_Instances.assign(std::begin(Instances), std::end(Instances));
    m_RegistryModule->UpdateInstances(GetIndex(), m_Instances);
}

void luvk::InstancedMesh::AddInstance(const MeshRegistry::InstanceInfo& Instance)
{
    m_Instances.push_back(Instance);
    m_RegistryModule->UpdateInstances(GetIndex(), m_Instances);
}
