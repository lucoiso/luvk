// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/MeshRegistry.hpp"
#include <memory>
#include <cstdint>
#include <vector>

namespace luvk
{
    /** Non-instanced mesh wrapper */
    class LUVKMODULE_API Mesh
    {
    protected:
        std::shared_ptr<luvk::MeshRegistry> m_Registry{};
        std::size_t m_Index{};

    public:
        constexpr Mesh() = default;

        explicit Mesh(std::shared_ptr<luvk::MeshRegistry> Registry, const std::size_t Index) noexcept : m_Registry(std::move(Registry)),
                                                                                                        m_Index(Index) {}

        [[nodiscard]] constexpr std::size_t GetIndex() const noexcept
        {
            return m_Index;
        }

        void Draw(VkCommandBuffer CommandBuffer) const;
    };

    /** Instanced mesh wrapper */
    class LUVKMODULE_API InstancedMesh : public Mesh
    {
        std::vector<luvk::MeshRegistry::InstanceInfo> m_Instances{};

    public:
        using Mesh::Mesh;

        void SetInstances(std::span<luvk::MeshRegistry::InstanceInfo const> Instances);
        void AddInstance(luvk::MeshRegistry::InstanceInfo const& Instance);
    };
} // namespace luvk
