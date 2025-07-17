// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Types/Mesh.hpp"
#include "luvk/Types/MeshDraw.hpp"

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
    luvk::RecordMeshCommands(CommandBuffer, Entry);
}

inline void luvk::InstancedMesh::SetInstances(std::span<MeshRegistry::InstanceInfo const> Instances)
{
    m_Instances.assign(std::begin(Instances), std::end(Instances));
    if (auto const& registry = GetRegistry())
    {
        registry->UpdateInstances(GetIndex(), m_Instances);
    }
}

inline void luvk::InstancedMesh::AddInstance(MeshRegistry::InstanceInfo const& Instance)
{
    m_Instances.push_back(Instance);
    if (auto const& registry = GetRegistry())
    {
        registry->UpdateInstances(GetIndex(), m_Instances);
    }
}
