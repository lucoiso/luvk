// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include <memory>
#include <cstdint>
#include <array>
#include <vector>

namespace luvk
{
    class Buffer;
    class Material;

    struct MeshInstance
    {
        std::array<float, 2> Offset{0.F, 0.F};
        float Angle{0.F};
        std::array<float, 4> Color{1.F, 1.F, 1.F, 1.F};
    };

    /** Internal data for a renderable mesh */
    struct LUVKMODULE_API MeshEntry
    {
        std::shared_ptr<Buffer> VertexBuffer{};  //!< Vertex buffer handle
        std::shared_ptr<Buffer> IndexBuffer{};   //!< Index buffer handle
        std::shared_ptr<Material> MaterialPtr{}; //!< Material information
        std::shared_ptr<Buffer> UniformBuffer{}; //!< Per-object uniform buffer
        /** Cached uniform contents for push constants */
        std::vector<std::byte> UniformCache{};
        std::shared_ptr<Buffer> InstanceBuffer{}; //!< Per-instance buffer
        std::uint32_t IndexCount{};               //!< Number of indices
        std::uint32_t InstanceCount{};            //!< Number of instances
    };
} // namespace luvk
