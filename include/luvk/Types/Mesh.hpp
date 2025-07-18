// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <vector>
#include "luvk/Module.hpp"
#include "luvk/Modules/MeshRegistry.hpp"

namespace luvk
{
    class LUVKMODULE_API Mesh
    {
        std::shared_ptr<MeshRegistry> m_Registry{};
        std::size_t m_Index{};

    public:
        constexpr Mesh() = default;

        explicit Mesh(std::shared_ptr<MeshRegistry> Registry, const std::size_t Index) noexcept : m_Registry(std::move(Registry)),
                                                                                                        m_Index(Index) {}

        [[nodiscard]] constexpr std::size_t GetIndex() const noexcept
        {
            return m_Index;
        }

        void Draw(VkCommandBuffer CommandBuffer) const;

    protected:
        [[nodiscard]] constexpr std::shared_ptr<MeshRegistry> const& GetRegistry() const noexcept
        {
            return m_Registry;
        }
    };

    class LUVKMODULE_API InstancedMesh : public Mesh
    {
        std::vector<MeshRegistry::InstanceInfo> m_Instances{};

    public:
        using Mesh::Mesh;

        void SetInstances(std::span<MeshRegistry::InstanceInfo const> Instances);
        void AddInstance(MeshRegistry::InstanceInfo const& Instance);
    };
} // namespace luvk
