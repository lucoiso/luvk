// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "ExampleApp.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <luvk/App/DebugDraw.hpp>
#include <luvk/App/MeshFeature.hpp>
#include <luvk/Core/Module.hpp>
#include <luvk/Resources/AccelerationStructure.hpp>
#include <luvk/Scene/Material.hpp>
#include "ExampleUi.hpp"
#include "Gallery.hpp"
#include "MeshShaderCube.hpp"
#include "Particles.hpp"
#include "RayTracingPreview.hpp"
#include "ShaderPlayground.hpp"
#include "Skybox.hpp"

namespace
{
    using luvk::scene::Material;
    using luvk::scene::MeshData;
    using luvk::scene::Shading;

    /** Two-tone checker (RGBA8, sRGB) for the floor. */
    std::vector<std::byte> CheckerPixels(const std::uint32_t Size, const std::uint32_t Tile)
    {
        std::vector<std::byte> Pixels;
        Pixels.reserve(static_cast<std::size_t>(Size) * Size * 4U);
        for (std::uint32_t Y = 0U; Y < Size; ++Y)
        {
            for (std::uint32_t X = 0U; X < Size; ++X)
            {
                const bool          Light = ((X / Tile) + (Y / Tile)) % 2U == 0U;
                const std::uint8_t  Value = Light ? 200U : 118U;
                Pixels.insert(std::end(Pixels), {std::byte{Value}, std::byte{Value}, static_cast<std::byte>(Value + 6U), std::byte{255U}});
            }
        }
        return Pixels;
    }

    /** Vertical quad facing +Z, half size 1, with the texture origin at the top-left corner (images read upright). */
    MeshData ScreenQuad()
    {
        MeshData Result;
        Result.Vertices = {{.Position = {-1.F, 1.F, 0.F}, .Normal = {0.F, 0.F, 1.F}, .TexCoord = {0.F, 0.F}},
                           {.Position = {1.F, 1.F, 0.F}, .Normal = {0.F, 0.F, 1.F}, .TexCoord = {1.F, 0.F}},
                           {.Position = {1.F, -1.F, 0.F}, .Normal = {0.F, 0.F, 1.F}, .TexCoord = {1.F, 1.F}},
                           {.Position = {-1.F, -1.F, 0.F}, .Normal = {0.F, 0.F, 1.F}, .TexCoord = {0.F, 1.F}}};
        Result.Indices  = {3U, 2U, 1U, 3U, 1U, 0U};
        return Result;
    }

    glm::mat4 Place(const glm::vec3& Position, const glm::vec3& Scale)
    {
        return glm::scale(glm::translate(glm::mat4{1.F}, Position), Scale);
    }
}

Example::ExampleApp::ExampleApp()
    : Application({.Title          = "luvk gallery",
                   .Width          = 1600U,
                   .Height         = 900U,
                   .ShowStatistics = false,
                   .Modules        = {std::make_shared<luvk::MeshShaderModule>(false), std::make_shared<luvk::RayTracingModule>(false)}})
{
}

Example::ExampleApp::~ExampleApp() = default;

void Example::ExampleApp::OnInit()
{
    luvk::Context& Ctx = GetContext();

    // Feature order inside a stage is the draw order: the sky first, then everything over it.
    m_Skybox     = Features().Add<Skybox>(LinearSampler().Index());
    m_Meshes     = Features().Add<luvk::app::MeshFeature>();
    m_Cube       = Features().Add<MeshShaderCube>(Gallery::MeshShaderCube);
    m_Fountain   = Features().Add<Particles>(Gallery::Fountain);
    m_RayTracing = Features().Add<RayTracingPreview>();
    m_ShaderLab  = Features().Add<ShaderPlayground>();
    m_DebugDraw  = Features().Add<luvk::app::DebugDraw>();

    const std::vector<std::byte> Checker = CheckerPixels(256U, 32U);
    m_Checker = luvk::scene::Texture::FromPixels(Ctx, Checker, 256U, 256U, {.Name = "floor checker"});
    m_Floor   = luvk::scene::Mesh(Ctx, MeshData::Plane(glm::vec2{12.F}, 12.F), "floor");
    m_Box     = luvk::scene::Mesh(Ctx, MeshData::Box(), "box");
    m_Sphere  = luvk::scene::Mesh(Ctx, MeshData::Sphere(), "sphere");
    m_Screen  = luvk::scene::Mesh(Ctx, ScreenQuad(), "screen");

    m_Ui = std::make_unique<ExampleUi>(*this,
                                       GalleryExhibits{.Sky        = *m_Skybox,
                                                       .Meshes     = *m_Meshes,
                                                       .Cube       = *m_Cube,
                                                       .Fountain   = *m_Fountain,
                                                       .RayTracing = *m_RayTracing,
                                                       .ShaderLab  = *m_ShaderLab,
                                                       .Debug      = *m_DebugDraw});

    luvk::app::Camera& Camera = GetCamera();
    Camera.Position           = Gallery::CameraStart;
    Camera.Pitch              = -14.F;
}

void Example::ExampleApp::OnShutdown()
{
    ReleaseUiTexture(m_ShaderLab->Output());
    if (m_RayTracing->IsAvailable())
    {
        ReleaseUiTexture(m_RayTracing->Output());
    }
}

void Example::ExampleApp::OnUpdate(const float DeltaTime)
{
    m_Time += DeltaTime;
    DrawMeshes();
    DrawDebugShapes();
}

void Example::ExampleApp::OnGui()
{
    m_Ui->Draw();
}

void Example::ExampleApp::DrawMeshes()
{
    luvk::app::MeshFeature& Meshes = *m_Meshes;

    Material Floor{.BaseColor = {0.85F, 0.85F, 0.87F, 1.F}, .Roughness = 0.85F};
    Meshes.Draw(m_Floor, Floor.WithTexture(m_Checker), glm::mat4{1.F});

    const Material Stone{.BaseColor = {0.42F, 0.43F, 0.47F, 1.F}, .Roughness = 0.7F};
    Meshes.Draw(m_Box, Stone, Place({Gallery::MeshShaderCube.x, 0.4F, Gallery::MeshShaderCube.z}, {0.9F, 0.8F, 0.9F}));
    Meshes.Draw(m_Box, Stone, Place({Gallery::Glass.x, 0.25F, Gallery::Glass.z}, {1.1F, 0.5F, 1.1F}));

    // Screens: a dark frame and an unlit quad showing a texture written earlier in the same frame.
    const Material Frame{.BaseColor = {0.06F, 0.06F, 0.07F, 1.F}, .Roughness = 0.3F};
    const auto     Screen = [&](const glm::vec3& Center, luvk::Image* Image)
    {
        const glm::vec2 Half = Gallery::ScreenHalfSize;
        Meshes.Draw(m_Box, Frame, Place(Center - glm::vec3{0.F, 0.F, 0.06F}, {Half.x * 2.F + 0.2F, Half.y * 2.F + 0.2F, 0.08F}));
        Meshes.Draw(m_Box, Frame, Place({Center.x, (Center.y - Half.y) * 0.5F, Center.z - 0.06F}, {0.15F, Center.y - Half.y, 0.15F}));

        const Material Display{.Model = Shading::Unlit, .BaseColor = Image != nullptr ? glm::vec4{1.F} : glm::vec4{0.1F, 0.1F, 0.12F, 1.F}, .BaseColorTexture = Image};
        Meshes.Draw(m_Screen, Display, Place(Center, {Half.x, Half.y, 1.F}));
    };
    Screen(Gallery::RayTracingScreen, m_RayTracing->IsAvailable() ? &m_RayTracing->Output() : nullptr);
    Screen(Gallery::ShaderLabScreen, &m_ShaderLab->Output());

    // Spheres: same mesh, five materials from glossy to rough.
    constexpr std::array<glm::vec3, 5> Colors{{{0.9F, 0.25F, 0.2F}, {0.95F, 0.7F, 0.2F}, {0.3F, 0.8F, 0.35F}, {0.25F, 0.55F, 0.95F}, {0.7F, 0.35F, 0.9F}}};
    for (std::size_t Index = 0U; Index < std::size(Colors); ++Index)
    {
        const float    Offset = (static_cast<float>(Index) - 2.F) * 1.5F;
        const Material Paint{.BaseColor = glm::vec4(Colors[Index], 1.F), .Roughness = static_cast<float>(Index) / 4.F};
        Meshes.Draw(m_Sphere, Paint, Place(Gallery::Spheres + glm::vec3{Offset, 0.F, 0.F}, glm::vec3{0.9F}));
    }

    // Instancing: one draw call for the whole ring.
    constexpr std::uint32_t RingCount = 36U;
    m_Ring.clear();
    for (std::uint32_t Index = 0U; Index < RingCount; ++Index)
    {
        const float     Angle    = glm::two_pi<float>() * static_cast<float>(Index) / static_cast<float>(RingCount) + m_Time * 0.4F;
        const glm::vec3 Position = Gallery::Instancing + glm::vec3{std::cos(Angle) * 1.4F, 0.35F + 0.15F * std::sin(Angle * 3.F + m_Time * 2.F), std::sin(Angle) * 1.4F};
        m_Ring.push_back(glm::rotate(Place(Position, glm::vec3{0.28F}), Angle + m_Time, glm::vec3{0.3F, 1.F, 0.2F}));
    }
    Meshes.DrawInstanced(m_Box, Material{.BaseColor = {1.F, 0.55F, 0.15F, 1.F}, .Emissive = {0.15F, 0.05F, 0.F}, .Roughness = 0.35F}, m_Ring);

    // Glass: blended after every opaque mesh, both faces visible.
    const Material Glass{.BaseColor = {0.55F, 0.85F, 1.F, 0.28F}, .Roughness = 0.05F, .Transparent = true, .DoubleSided = true};
    Meshes.Draw(m_Box, Glass, glm::rotate(Place(Gallery::Glass, glm::vec3{0.9F}), m_Time * 0.5F, glm::vec3{0.F, 1.F, 0.F}));
}

void Example::ExampleApp::DrawDebugShapes()
{
    luvk::app::DebugDraw& Debug = *m_DebugDraw;

    if (m_Ui->DrawGrid())
    {
        Debug.Grid(12.F, 24, {0.35F, 0.35F, 0.4F, 0.5F});
    }

    if (m_Ui->DrawAxes())
    {
        Debug.Axes(glm::mat4{1.F}, 1.F);
    }

    if (m_Ui->DrawBounds() && m_Cube->IsEnabled())
    {
        Debug.Box(glm::vec3{-0.5F}, glm::vec3{0.5F}, {1.F, 1.F, 0.F, 1.F}, m_Cube->Transform());
    }

    if (const std::optional<glm::mat4>& Frustum = m_Ui->FrozenFrustum())
    {
        Debug.Frustum(*Frustum, {0.2F, 1.F, 0.4F, 1.F});
    }
}
