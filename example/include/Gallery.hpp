// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <glm/glm.hpp>

/** World layout of the gallery: where each exhibit stands (Y up, the camera starts on +Z looking towards -Z). */
namespace Example::Gallery
{
    /** Screen showing the ray traced Cornell box. */
    inline constexpr glm::vec3 RayTracingScreen{-2.2F, 1.7F, -4.F};

    /** Screen showing the Shader Lab output. */
    inline constexpr glm::vec3 ShaderLabScreen{2.2F, 1.7F, -4.F};

    /** Half size of both screens. */
    inline constexpr glm::vec2 ScreenHalfSize{1.4F, 1.4F};

    /** Mesh shader cube, floating over its pedestal. */
    inline constexpr glm::vec3 MeshShaderCube{0.F, 1.35F, -2.4F};

    /** Row of lit spheres with increasing roughness. */
    inline constexpr glm::vec3 Spheres{0.F, 0.45F, 0.F};

    /** Ring of instanced cubes. */
    inline constexpr glm::vec3 Instancing{-4.6F, 0.F, 1.6F};

    /** Particle fountain. */
    inline constexpr glm::vec3 Fountain{4.6F, 0.F, 1.6F};

    /** Transparent glass box on a pedestal. */
    inline constexpr glm::vec3 Glass{0.F, 0.95F, 2.8F};

    /** Where the camera starts. */
    inline constexpr glm::vec3 CameraStart{0.F, 2.4F, 8.F};
}
