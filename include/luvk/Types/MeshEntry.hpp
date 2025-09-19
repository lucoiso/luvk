// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include "luvk/Module.hpp"
#include "luvk/Types/Vector.hpp"

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

    struct LUVKMODULE_API MeshEntry
    {
        std::shared_ptr<Buffer> VertexBuffer{};
        std::shared_ptr<Buffer> IndexBuffer{};
        std::shared_ptr<Buffer> UniformBuffer{};
        std::shared_ptr<Buffer> InstanceBuffer{};
        std::shared_ptr<Material> MaterialPtr{};
        Vector<std::byte> UniformCache{};
        std::uint32_t IndexCount{};
        std::uint32_t InstanceCount{};
        std::uint32_t DispatchX{1};
        std::uint32_t DispatchY{1};
        std::uint32_t DispatchZ{1};
    };
} // namespace luvk
