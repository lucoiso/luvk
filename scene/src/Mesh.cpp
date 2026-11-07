// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Scene/Mesh.hpp"
#include <array>
#include <stdexcept>
#include <string>
#include <glm/gtc/constants.hpp>

luvk::scene::MeshData luvk::scene::MeshData::Box(const glm::vec3& HalfExtent)
{
    // One quad per face so every face has its own normal and texture coordinates.
    struct Face
    {
        glm::vec3 Normal;
        glm::vec3 Tangent;
        glm::vec3 Bitangent;
    };

    constexpr std::array<Face, 6> Faces{{{{1.F, 0.F, 0.F}, {0.F, 0.F, -1.F}, {0.F, 1.F, 0.F}},
                                         {{-1.F, 0.F, 0.F}, {0.F, 0.F, 1.F}, {0.F, 1.F, 0.F}},
                                         {{0.F, 1.F, 0.F}, {1.F, 0.F, 0.F}, {0.F, 0.F, -1.F}},
                                         {{0.F, -1.F, 0.F}, {1.F, 0.F, 0.F}, {0.F, 0.F, 1.F}},
                                         {{0.F, 0.F, 1.F}, {1.F, 0.F, 0.F}, {0.F, 1.F, 0.F}},
                                         {{0.F, 0.F, -1.F}, {-1.F, 0.F, 0.F}, {0.F, 1.F, 0.F}}}};

    MeshData Result;
    for (const Face& It : Faces)
    {
        const auto Base = static_cast<std::uint32_t>(std::size(Result.Vertices));
        for (const glm::vec2 Corner : {glm::vec2{-1.F, -1.F}, glm::vec2{1.F, -1.F}, glm::vec2{1.F, 1.F}, glm::vec2{-1.F, 1.F}})
        {
            const glm::vec3 Position = (It.Normal + It.Tangent * Corner.x + It.Bitangent * Corner.y) * HalfExtent;
            Result.Vertices.push_back({.Position = Position, .Normal = It.Normal, .TexCoord = Corner * 0.5F + 0.5F});
        }
        Result.Indices.insert(std::end(Result.Indices), {Base, Base + 1U, Base + 2U, Base, Base + 2U, Base + 3U});
    }
    return Result;
}

luvk::scene::MeshData luvk::scene::MeshData::Sphere(const float Radius, const std::uint32_t Rings, const std::uint32_t Segments)
{
    MeshData Result;
    for (std::uint32_t Ring = 0U; Ring <= Rings; ++Ring)
    {
        const float V   = static_cast<float>(Ring) / static_cast<float>(Rings);
        const float Phi = V * glm::pi<float>();
        for (std::uint32_t Segment = 0U; Segment <= Segments; ++Segment)
        {
            const float     U      = static_cast<float>(Segment) / static_cast<float>(Segments);
            const float     Theta  = U * glm::two_pi<float>();
            const glm::vec3 Normal = {std::sin(Phi) * std::cos(Theta), std::cos(Phi), std::sin(Phi) * std::sin(Theta)};
            Result.Vertices.push_back({.Position = Normal * Radius, .Normal = Normal, .TexCoord = {U, V}});
        }
    }

    const std::uint32_t Stride = Segments + 1U;
    for (std::uint32_t Ring = 0U; Ring < Rings; ++Ring)
    {
        for (std::uint32_t Segment = 0U; Segment < Segments; ++Segment)
        {
            const std::uint32_t A = Ring * Stride + Segment;
            const std::uint32_t B = A + Stride;
            Result.Indices.insert(std::end(Result.Indices), {A, A + 1U, B, B, A + 1U, B + 1U});
        }
    }
    return Result;
}

luvk::scene::MeshData luvk::scene::MeshData::Plane(const glm::vec2& HalfExtent, const float UvScale)
{
    MeshData Result;
    for (const glm::vec2 Corner : {glm::vec2{-1.F, -1.F}, glm::vec2{1.F, -1.F}, glm::vec2{1.F, 1.F}, glm::vec2{-1.F, 1.F}})
    {
        Result.Vertices.push_back({.Position = {Corner.x * HalfExtent.x, 0.F, Corner.y * HalfExtent.y},
                                   .Normal   = {0.F, 1.F, 0.F},
                                   .TexCoord = (Corner * 0.5F + 0.5F) * UvScale});
    }
    Result.Indices = {0U, 2U, 1U, 0U, 3U, 2U};
    return Result;
}

luvk::scene::Bounds luvk::scene::MeshData::ComputeBounds() const
{
    if (std::empty(Vertices))
    {
        return {};
    }

    Bounds Result{.Min = Vertices.front().Position, .Max = Vertices.front().Position};
    for (const Vertex& It : Vertices)
    {
        Result.Min = glm::min(Result.Min, It.Position);
        Result.Max = glm::max(Result.Max, It.Position);
    }
    return Result;
}

std::vector<float> luvk::scene::MeshData::Positions() const
{
    std::vector<float> Result;
    Result.reserve(std::size(Vertices) * 3U);
    for (const Vertex& It : Vertices)
    {
        Result.insert(std::end(Result), {It.Position.x, It.Position.y, It.Position.z});
    }
    return Result;
}

luvk::scene::Mesh::Mesh(Context& Ctx, const MeshData& Data, const std::string_view Name)
    : m_IndexCount(static_cast<std::uint32_t>(std::size(Data.Indices))),
      m_Submeshes(Data.Submeshes),
      m_Bounds(Data.ComputeBounds())
{
    if (std::empty(Data.Vertices) || std::size(Data.Indices) < 3U || std::size(Data.Indices) % 3U != 0U)
    {
        throw std::invalid_argument("[luvk-scene] A mesh needs vertices and a triangle list.");
    }

    const std::string Label(Name);
    m_Vertices = Buffer(Ctx, {.Size = std::size(Data.Vertices) * sizeof(Vertex), .Usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, .Name = Label + " vertices"});
    m_Indices  = Buffer(Ctx, {.Size = std::size(Data.Indices) * sizeof(std::uint32_t), .Usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT, .Name = Label + " indices"});
    m_Vertices.Write(std::span<const Vertex>(Data.Vertices));
    m_Indices.Write(std::span<const std::uint32_t>(Data.Indices));

    if (std::empty(m_Submeshes))
    {
        m_Submeshes.push_back({.FirstIndex = 0U, .IndexCount = m_IndexCount});
    }
}
