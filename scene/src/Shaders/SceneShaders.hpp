// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <span>

namespace luvk::scene::shaders
{
    /** SPIR-V of the mesh renderer vertex shader (vertex pulling, instanced). */
    [[nodiscard]] std::span<const std::uint32_t> MeshVertexShader();

    /** SPIR-V of the mesh renderer fragment shader (unlit / lit materials). */
    [[nodiscard]] std::span<const std::uint32_t> MeshFragmentShader();
}