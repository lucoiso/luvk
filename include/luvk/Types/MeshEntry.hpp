// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** MeshEntry.hpp definitions */

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
        /** Optional vertex buffer */
        std::shared_ptr<Buffer> VertexBuffer{};

        /** Optional index buffer */
        std::shared_ptr<Buffer> IndexBuffer{};

        /** Per-object uniform buffer */
        std::shared_ptr<Buffer> UniformBuffer{};

        /** Per-instance buffer */
        std::shared_ptr<Buffer> InstanceBuffer{};

        /** Material information */
        std::shared_ptr<Material> MaterialPtr{};

        /** Cached uniform contents for push constants */
        std::vector<std::byte> UniformCache{};

        /** Index count for indexed draws */
        std::uint32_t IndexCount{};

        /** Dispatch group counts used for compute and mesh shaders */
        std::uint32_t InstanceCount{}; //!< Number of instances
        std::uint32_t DispatchX{1};
        std::uint32_t DispatchY{1};
        std::uint32_t DispatchZ{1};
    };
} // namespace luvk
