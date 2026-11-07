// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <luvk/App/Feature.hpp>
#include <luvk/Resources/AccelerationStructure.hpp>
#include <luvk/Resources/Image.hpp>
#include <luvk/Resources/Pipeline.hpp>

namespace Example
{
    /** Hardware ray tracing through ray queries in a compute shader, written into a storage image that the UI
     *  displays (needs luvk::RayTracingModule). The scene is a Cornell box with a rotated cube and a mirror sphere. */
    class RayTracingPreview final : public luvk::app::RenderFeature
    {
    private:
        /** CPU-side mirror of the shader push constant block. */
        struct PushData
        {
            /** Bindless index of the top level acceleration structure. */
            std::uint32_t Scene;

            /** Bindless storage index of the output image. */
            std::uint32_t Image;

            /** Orbit animation time (seconds). */
            float Time;

            /** Tone mapping exposure. */
            float Exposure;

            /** Light position (xyz); w is unused. */
            float LightPosition[4];

            /** Light color (rgb) and intensity (a). */
            float LightColor[4];

            /** Sphere center (xyz) and radius (w). */
            float Sphere[4];

            /** Cube center (xyz) and Y rotation in radians (w). */
            float Cube[4];
        };

        /** Cornell-box walls, built in world space. */
        luvk::AccelerationStructure m_Box;

        /** Local-space cube mesh instanced by the top level structure. */
        luvk::AccelerationStructure m_Cube;

        /** Local-space unit sphere mesh instanced by the top level structure. */
        luvk::AccelerationStructure m_Sphere;

        /** Top level acceleration structure wrapping the three instances. */
        luvk::AccelerationStructure m_TopLevel;

        /** Storage image the compute shader writes and the UI displays. */
        luvk::Image m_Output;

        /** Compute pipeline tracing the rays. */
        luvk::Pipeline m_Pipeline;

        /** Whether the device supports ray queries and storage image writes. */
        bool m_Available = false;

        /** True while the camera orbit is animated. */
        bool m_Animate = true;

        /** Orbit speed multiplier. */
        float m_OrbitSpeed = 1.F;

        /** Accumulated orbit time in seconds. */
        float m_OrbitTime = 0.F;

        /** Tone mapping exposure. */
        float m_Exposure = 1.1F;

        /** Human-readable capability status shown in the debug window. */
        std::string m_Status = "not attached";

    public:
        /** Name shown in the debug window. */
        [[nodiscard]] std::string_view Name() const override { return "Ray Tracing (Cornell box)"; }

        /** Stage in the frame graph (GPU-driven work). */
        [[nodiscard]] luvk::app::FeatureStage Stage() const override { return luvk::app::FeatureStage::Compute; }

        /** Builds the acceleration structures, the output image and the compute pipeline. */
        void OnAttach(luvk::Context& Ctx) override;

        /** Advances the orbit animation. */
        void Update(float DeltaTime) override;

        /** Adds the ray tracing pass for this frame. */
        void AddPasses(luvk::app::FeatureContext& Frame) override;

        /** Draws the explanation and the orbit/exposure controls. */
        void OnGui() override;

        /** Storage image the UI can display. */
        [[nodiscard]] luvk::Image& Output() noexcept { return m_Output; }

        /** Whether the device supports ray queries and storage image writes. */
        [[nodiscard]] bool IsAvailable() const noexcept { return m_Available; }

        /** Human-readable capability status. */
        [[nodiscard]] const std::string& Status() const noexcept { return m_Status; }
    };
}
