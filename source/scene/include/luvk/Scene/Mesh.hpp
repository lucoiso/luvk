// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include <glm/glm.hpp>
#include <luvk/Resources/Buffer.hpp>

namespace luvk
{
    class Context;
}

namespace luvk::scene
{
    /** Standard mesh vertex: 8 tightly packed floats, read by the mesh shaders through the buffer device address. */
    struct Vertex
    {
        /** Object-space position. */
        glm::vec3 Position{0.F};

        /** Object-space normal. */
        glm::vec3 Normal{0.F, 1.F, 0.F};

        /** Texture coordinate. */
        glm::vec2 TexCoord{0.F};
    };

    static_assert(sizeof(Vertex) == 32U, "luvk::scene::Vertex must stay 8 tightly packed floats (shader layout).");

    /** Range of indices drawn with one material. */
    struct Submesh
    {
        /** First index of the range. */
        std::uint32_t FirstIndex = 0U;

        /** Number of indices of the range. */
        std::uint32_t IndexCount = 0U;
    };

    /** Axis-aligned bounding box. */
    struct Bounds
    {
        /** Minimum corner. */
        glm::vec3 Min{0.F};

        /** Maximum corner. */
        glm::vec3 Max{0.F};
    };

    /**
     * CPU-side geometry: what a Mesh is built from, and what acceleration structures are built from
     * (AccelerationStructure::BuildBottomLevel(Ctx, {.Positions = Data.Positions(), .Indices = Data.Indices})).
     */
    struct MeshData
    {
        /** Vertices of the mesh. */
        std::vector<Vertex> Vertices;

        /** Triangle list indices. */
        std::vector<std::uint32_t> Indices;

        /** Index ranges; empty means one range covering every index. */
        std::vector<Submesh> Submeshes;

        /** Axis-aligned box centred on the origin. */
        [[nodiscard]] static MeshData Box(const glm::vec3& HalfExtent = glm::vec3{0.5F});

        /** UV sphere centred on the origin. */
        [[nodiscard]] static MeshData Sphere(float Radius = 0.5F, std::uint32_t Rings = 24U, std::uint32_t Segments = 48U);

        /** XZ plane facing +Y, centred on the origin; UvScale repeats the texture across it. */
        [[nodiscard]] static MeshData Plane(const glm::vec2& HalfExtent = glm::vec2{0.5F}, float UvScale = 1.F);

        /** Bounds of every vertex. */
        [[nodiscard]] Bounds ComputeBounds() const;

        /** Positions as xyz floats (the layout acceleration structure builds take). */
        [[nodiscard]] std::vector<float> Positions() const;
    };

    /** Geometry in device-local memory, drawn by the MeshRenderer (vertex pulling, 32-bit indices). */
    class Mesh
    {
    public:
        /** Empty mesh. */
        Mesh() = default;

        /** Uploads Data (which must contain at least one triangle). */
        Mesh(Context& Ctx, const MeshData& Data, std::string_view Name = {});

        /** True when the mesh holds geometry. */
        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(m_Vertices); }

        /** Vertex buffer (luvk::scene::Vertex records). */
        [[nodiscard]] const Buffer& Vertices() const noexcept { return m_Vertices; }

        /** Index buffer (uint32 triangle list). */
        [[nodiscard]] const Buffer& Indices() const noexcept { return m_Indices; }

        /** Number of indices. */
        [[nodiscard]] std::uint32_t IndexCount() const noexcept { return m_IndexCount; }

        /** Index ranges (at least one). */
        [[nodiscard]] std::span<const Submesh> Submeshes() const noexcept { return m_Submeshes; }

        /** Object-space bounds. */
        [[nodiscard]] const Bounds& GetBounds() const noexcept { return m_Bounds; }

    private:
        /** Vertex buffer. */
        Buffer m_Vertices;

        /** Index buffer. */
        Buffer m_Indices;

        /** Number of indices. */
        std::uint32_t m_IndexCount = 0U;

        /** Index ranges. */
        std::vector<Submesh> m_Submeshes;

        /** Object-space bounds. */
        Bounds m_Bounds;
    };
}
