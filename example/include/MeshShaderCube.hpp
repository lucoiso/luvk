// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string>
#include <string_view>
#include <glm/glm.hpp>
#include <luvk/App/Feature.hpp>
#include <luvk/Resources/Pipeline.hpp>

namespace Example
{
    /** Mesh shading: a task shader launches one mesh workgroup per cube face (needs VK_EXT_mesh_shader). */
    class MeshShaderCube final : public luvk::app::RenderFeature
    {
    public:
        /** Constructor: places the cube at Position. */
        explicit MeshShaderCube(const glm::vec3& Position) : m_Position(Position) {}

    private:
        /** World position of the cube centre. */
        glm::vec3 m_Position{0.F};

        /** Mesh pipeline (recreated when the color format or the wireframe setting changes). */
        luvk::Pipeline m_Pipeline;

        /** Color format the pipeline was built for. */
        VkFormat m_Format = VK_FORMAT_UNDEFINED;

        /** Wireframe setting the pipeline was built for. */
        bool m_Wireframe = false;

        /** Reverse depth setting the pipeline was built for. */
        bool m_ReverseDepth = false;

        /** Current rotation angle in radians. */
        float m_Angle = 0.F;

        /** Rotation speed in radians per second (tuned from the UI). */
        float m_Speed = 0.8F;

        /** Whether the device supports the mesh shader extension. */
        bool m_Available = false;

        /** Human-readable capability status shown in the debug window. */
        std::string m_Status = "not attached";

    public:
        /** Name shown in the debug window. */
        [[nodiscard]] std::string_view Name() const override { return "Mesh Shader Cube"; }

        /** Stage in the frame graph (opaque geometry). */
        [[nodiscard]] luvk::app::FeatureStage Stage() const override { return luvk::app::FeatureStage::Opaque; }

        /** Checks the mesh shader feature requirements. */
        void OnAttach(luvk::Context& Ctx) override;

        /** Advances the rotation by DeltaTime seconds. */
        void Update(float DeltaTime) override;

        /** Adds the cube pass for this frame. */
        void AddPasses(luvk::app::FeatureContext& Frame) override;

        /** Draws the rotation speed slider. */
        void OnGui() override;

        /** Model matrix rotating the cube around two axes. */
        [[nodiscard]] glm::mat4 Transform() const;

        /** Whether VK_EXT_mesh_shader is supported by the device. */
        [[nodiscard]] bool IsAvailable() const noexcept { return m_Available; }

        /** Human-readable capability status. */
        [[nodiscard]] const std::string& Status() const noexcept { return m_Status; }
    };
}
