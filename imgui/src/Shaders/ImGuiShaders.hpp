// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <span>

namespace luvk::imgui::shaders
{
    /** SPIR-V of the Dear ImGui vertex shader. */
    [[nodiscard]] std::span<const std::uint32_t> ImGuiVertexShader();

    /** SPIR-V of the Dear ImGui fragment shader. */
    [[nodiscard]] std::span<const std::uint32_t> ImGuiFragmentShader();
}