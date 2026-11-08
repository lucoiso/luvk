// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include <type_traits>
#include "RayTracingPreview.hpp"
#include <array>
#include <bit>
#include <stdexcept>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <luvk/Core/Context.hpp>
#include <luvk/Core/FeatureChain.hpp>
#include <luvk/Render/CommandList.hpp>
#include <luvk/Shaders/ShaderCompiler.hpp>
#include "Shaders/RayTracingPreview.hpp"

namespace
{
    using Shaders::RayTracingPreview::Shader;

    /** Triangle soup (tightly packed xyz positions) used to build the bottom-level structures. */
    using TriangleSoup = std::vector<float>;

    constexpr std::uint32_t PreviewSize = 512U;
    constexpr float         CubeHalfExtent = 0.34F;
    constexpr float         CubeRotation   = 0.55F;
    constexpr float         SphereRadius   = 0.36F;

    void AddTriangle(TriangleSoup& Soup, const glm::vec3& A, const glm::vec3& B, const glm::vec3& C)
    {
        for (const glm::vec3& Vertex : {A, B, C})
        {
            Soup.push_back(Vertex.x);
            Soup.push_back(Vertex.y);
            Soup.push_back(Vertex.z);
        }
    }

    void AddQuad(TriangleSoup& Soup, const glm::vec3& A, const glm::vec3& B, const glm::vec3& C, const glm::vec3& D)
    {
        AddTriangle(Soup, A, B, C);
        AddTriangle(Soup, A, C, D);
    }

    TriangleSoup MakeBoxSoup(const glm::vec3& Min, const glm::vec3& Max)
    {
        TriangleSoup Soup;
        // Floor, ceiling, back, left and right walls. The front (z = 1) stays open so the camera can look inside.
        AddQuad(Soup, {Min.x, Min.y, Min.z}, {Max.x, Min.y, Min.z}, {Max.x, Min.y, Max.z}, {Min.x, Min.y, Max.z});
        AddQuad(Soup, {Min.x, Max.y, Min.z}, {Max.x, Max.y, Min.z}, {Max.x, Max.y, Max.z}, {Min.x, Max.y, Max.z});
        AddQuad(Soup, {Min.x, Min.y, Min.z}, {Max.x, Min.y, Min.z}, {Max.x, Max.y, Min.z}, {Min.x, Max.y, Min.z});
        AddQuad(Soup, {Min.x, Min.y, Min.z}, {Min.x, Min.y, Max.z}, {Min.x, Max.y, Max.z}, {Min.x, Max.y, Min.z});
        AddQuad(Soup, {Max.x, Min.y, Min.z}, {Max.x, Min.y, Max.z}, {Max.x, Max.y, Max.z}, {Max.x, Max.y, Min.z});
        return Soup;
    }

    TriangleSoup MakeCubeSoup(const glm::vec3& HalfExtent)
    {
        TriangleSoup Soup;
        const glm::vec3 Min = -HalfExtent;
        const glm::vec3 Max = HalfExtent;

        AddQuad(Soup, {Min.x, Min.y, Min.z}, {Max.x, Min.y, Min.z}, {Max.x, Max.y, Min.z}, {Min.x, Max.y, Min.z});
        AddQuad(Soup, {Min.x, Min.y, Max.z}, {Max.x, Min.y, Max.z}, {Max.x, Max.y, Max.z}, {Min.x, Max.y, Max.z});
        AddQuad(Soup, {Min.x, Min.y, Min.z}, {Min.x, Min.y, Max.z}, {Min.x, Max.y, Max.z}, {Min.x, Max.y, Min.z});
        AddQuad(Soup, {Max.x, Min.y, Min.z}, {Max.x, Min.y, Max.z}, {Max.x, Max.y, Max.z}, {Max.x, Max.y, Min.z});
        AddQuad(Soup, {Min.x, Min.y, Min.z}, {Max.x, Min.y, Min.z}, {Max.x, Min.y, Max.z}, {Min.x, Min.y, Max.z});
        AddQuad(Soup, {Min.x, Max.y, Min.z}, {Max.x, Max.y, Min.z}, {Max.x, Max.y, Max.z}, {Min.x, Max.y, Max.z});
        return Soup;
    }

    TriangleSoup MakeSphereSoup(const std::uint32_t Rings, const std::uint32_t Segments)
    {
        TriangleSoup Soup;
        for (std::uint32_t Ring = 0U; Ring < Rings; ++Ring)
        {
            const float V0 = static_cast<float>(Ring) / static_cast<float>(Rings);
            const float V1 = static_cast<float>(Ring + 1U) / static_cast<float>(Rings);
            const float Phi0 = V0 * glm::pi<float>();
            const float Phi1 = V1 * glm::pi<float>();

            for (std::uint32_t Segment = 0U; Segment < Segments; ++Segment)
            {
                const float U0 = static_cast<float>(Segment) / static_cast<float>(Segments);
                const float U1 = static_cast<float>(Segment + 1U) / static_cast<float>(Segments);
                const float Theta0 = U0 * glm::two_pi<float>();
                const float Theta1 = U1 * glm::two_pi<float>();

                const auto Point = [](const float Phi, const float Theta)
                {
                    return glm::vec3{std::sin(Phi) * std::cos(Theta), std::cos(Phi), std::sin(Phi) * std::sin(Theta)};
                };

                const glm::vec3 A = Point(Phi0, Theta0);
                const glm::vec3 B = Point(Phi1, Theta0);
                const glm::vec3 C = Point(Phi1, Theta1);
                const glm::vec3 D = Point(Phi0, Theta1);
                AddQuad(Soup, A, B, C, D);
            }
        }
        return Soup;
    }

    /** Converts a glm transform to the row-major 3x4 layout used by VkAccelerationStructureInstanceKHR. */
    std::array<float, 12> ToInstanceTransform(const glm::mat4& Matrix)
    {
        return {Matrix[0][0], Matrix[1][0], Matrix[2][0], Matrix[3][0],
                Matrix[0][1], Matrix[1][1], Matrix[2][1], Matrix[3][1],
                Matrix[0][2], Matrix[1][2], Matrix[2][2], Matrix[3][2]};
    }

    glm::mat4 CubeTransform()
    {
        glm::mat4 Transform = glm::translate(glm::mat4{1.F}, glm::vec3{-0.42F, CubeHalfExtent, -0.62F});
        return glm::rotate(Transform, CubeRotation, glm::vec3{0.F, 1.F, 0.F});
    }

    glm::mat4 SphereTransform()
    {
        glm::mat4 Transform = glm::translate(glm::mat4{1.F}, glm::vec3{0.36F, SphereRadius, -0.82F});
        return glm::scale(Transform, glm::vec3{SphereRadius});
    }
}

void Example::RayTracingPreview::OnAttach(luvk::Context& Ctx)
{
    m_Available = luvk::RayTracingModule::IsAvailable(Ctx) && Ctx.EnabledFeatures().Core().shaderStorageImageWriteWithoutFormat != VK_FALSE;
    if (!m_Available)
    {
        m_Status = "unavailable: the device does not support ray queries (VK_KHR_ray_query)";
        SetEnabled(false);
        return;
    }

    try
    {
        const TriangleSoup BoxSoup    = MakeBoxSoup({-1.F, 0.F, -2.F}, {1.F, 2.F, 1.F});
        const TriangleSoup CubeSoup   = MakeCubeSoup(glm::vec3{CubeHalfExtent});
        const TriangleSoup SphereSoup = MakeSphereSoup(18U, 28U);

        m_Box    = luvk::AccelerationStructure::BuildBottomLevel(Ctx, {.Positions = BoxSoup}, "example cornell box");
        m_Cube   = luvk::AccelerationStructure::BuildBottomLevel(Ctx, {.Positions = CubeSoup}, "example cube");
        m_Sphere = luvk::AccelerationStructure::BuildBottomLevel(Ctx, {.Positions = SphereSoup}, "example sphere");

        const std::array Instances{
            luvk::TopLevelInstance{.BottomLevel = &m_Box, .Transform = ToInstanceTransform(glm::mat4{1.F}), .CustomIndex = 0U},
            luvk::TopLevelInstance{.BottomLevel = &m_Cube, .Transform = ToInstanceTransform(CubeTransform()), .CustomIndex = 1U},
            luvk::TopLevelInstance{.BottomLevel = &m_Sphere, .Transform = ToInstanceTransform(SphereTransform()), .CustomIndex = 2U}};
        m_TopLevel = luvk::AccelerationStructure::BuildTopLevel(Ctx, Instances, "example tlas");

        // Traced into mip 0, then the mip chain is generated every frame: the gallery screen samples it minified.
        m_Output = luvk::Image(Ctx,
                               {.Extent    = {PreviewSize, PreviewSize, 1U},
                                .Format    = luvk::Format::R8G8B8A8Unorm,
                                .MipLevels = static_cast<std::uint32_t>(std::bit_width(PreviewSize)),
                                .Name      = "ray tracing output"});

        m_Pipeline = luvk::Pipeline(Ctx, luvk::ComputePipelineDesc{.Compute = {.Code = luvk::CompileShaderOrThrow(Shader, "ray_query", "ComputeMain")}, .Name = "ray query"});
        m_Status   = "ready: 3 BLAS (Cornell box, cube, sphere), 3 TLAS instances, 512x512 primary + shadow + reflection rays";
    }
    catch (const std::exception& Exception)
    {
        m_Available = false;
        m_Status    = std::string("initialization failed: ") + Exception.what();
        SetEnabled(false);
    }
}

void Example::RayTracingPreview::Update(const float DeltaTime)
{
    if (m_Animate)
    {
        m_OrbitTime += DeltaTime * m_OrbitSpeed;
    }
}

void Example::RayTracingPreview::AddPasses(luvk::frame::FeatureContext& Frame)
{
    static_assert(std::is_trivially_copyable_v<PushData>);
    static_assert(sizeof(PushData) <= luvk::PushConstantSize);
    if (!m_Available)
    {
        return;
    }

    const PushData Push{.Scene         = m_TopLevel.BindlessIndex(),
                        .Image         = m_Output.Subview(0U).StorageIndex,
                        .Time          = m_OrbitTime,
                        .Exposure      = m_Exposure,
                        .LightPosition = {0.15F, 1.88F, -0.55F, 0.F},
                        .LightColor    = {1.F, 0.82F, 0.62F, 1.35F},
                        .Sphere        = {0.36F, SphereRadius, -0.82F, SphereRadius},
                        .Cube          = {-0.42F, CubeHalfExtent, -0.62F, CubeRotation}};

    const luvk::TextureHandle Output = Frame.Graph.Import(m_Output);

    Frame.Graph.AddPass("Ray Tracing",
                        luvk::PassType::Compute,
                        [&](luvk::PassBuilder& Pass)
                        {
                            Pass.Use(Output.Mip(0U), luvk::Access::ShaderStorageWrite);
                        },
                        [this, Push](luvk::CommandList& Cmd)
                        {
                            Cmd.BindPipeline(m_Pipeline);
                            Cmd.PushConstants(Push);
                            Cmd.Dispatch(PreviewSize / 8U, PreviewSize / 8U);
                        });

    Frame.Graph.GenerateMips(Output);
}

void Example::RayTracingPreview::OnGui()
{
    if (!m_Available)
    {
        ImGui::TextWrapped("%s", m_Status.c_str());
        return;
    }

    // The scene itself is described next to the image (ExampleApp's "Ray Tracing" window): only the controls here.
    ImGui::Checkbox("Animate orbit", &m_Animate);
    ImGui::SliderFloat("Orbit speed", &m_OrbitSpeed, 0.1F, 3.F);
    ImGui::SliderFloat("Exposure", &m_Exposure, 0.2F, 3.F);
    ImGui::TextDisabled("%s", m_Status.c_str());
}
