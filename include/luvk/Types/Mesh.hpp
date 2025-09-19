// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include "luvk/Module.hpp"
#include "luvk/Modules/MeshRegistry.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class LUVKMODULE_API Mesh
    {
        std::shared_ptr<MeshRegistry> m_Registry{};
        std::size_t m_Index{};

    public:
        constexpr Mesh() = default;

        explicit Mesh(const std::shared_ptr<MeshRegistry>& Registry, const std::size_t Index) noexcept : m_Registry(Registry),
                                                                                                         m_Index(Index) {}

        [[nodiscard]] constexpr std::size_t GetIndex() const noexcept
        {
            return m_Index;
        }

        void Draw(const VkCommandBuffer& CommandBuffer) const;

        [[nodiscard]] constexpr const std::shared_ptr<MeshRegistry>& GetRegistry() const noexcept
        {
            return m_Registry;
        }
    };

    class LUVKMODULE_API InstancedMesh : public Mesh
    {
        Vector<MeshRegistry::InstanceInfo> m_Instances{};

    public:
        using Mesh::Mesh;

        void SetInstances(const std::span<const MeshRegistry::InstanceInfo>& Instances);
        void AddInstance(const MeshRegistry::InstanceInfo& Instance);
    };
} // namespace luvk
