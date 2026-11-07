// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <span>

namespace luvk::app::shaders
{
    /** SPIR-V of the fullscreen composite vertex shader. */
    [[nodiscard]] std::span<const std::uint32_t> CompositeVertexShader();

    /** SPIR-V of the fullscreen composite fragment shader. */
    [[nodiscard]] std::span<const std::uint32_t> CompositeFragmentShader();

    /** SPIR-V of the debug draw line vertex shader. */
    [[nodiscard]] std::span<const std::uint32_t> DebugDrawVertexShader();

    /** SPIR-V of the debug draw line fragment shader. */
    [[nodiscard]] std::span<const std::uint32_t> DebugDrawFragmentShader();

    /** SPIR-V of the vertex shader pulling GPU-written debug lines through a buffer device address. */
    [[nodiscard]] std::span<const std::uint32_t> DebugDrawGpuVertexShader();
}