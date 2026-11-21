// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <string_view>
#include <glm/glm.hpp>
#include <luvk/Frame/Feature.hpp>
#include <luvk/Resources/Pipeline.hpp>
#include <luvk/Scene/Texture.hpp>

namespace Example
{
    /** Procedural sky cubemap (6-layer image, sampled through the bindless heap) drawn behind the scene. */
    class Skybox final : public luvk::frame::RenderFeature
    {
    public:
        /** CPU-side mirror of the shader push constant block. */
        struct PushData
        {
            /** Inverse view-projection of the active camera. */
            glm::mat4 InverseViewProjection;

            /** World position of the active camera. */
            glm::vec3 CameraPosition;

            /** Bindless index of the cubemap texture. */
            std::uint32_t Texture;

            /** Bindless index of the cubemap sampler. */
            std::uint32_t Sampler;
        };

    private:
        /** Procedural cubemap generated and uploaded once on attach. */
        luvk::scene::Texture m_Cubemap;

        /** Pipeline sampling the cubemap into SceneColor. */
        luvk::Pipeline m_Pipeline;

        /** Color format the pipeline was built for (rebuilt when the frame changes it). */
        luvk::Format m_Format = luvk::Format::Undefined;

        /** Bindless index of the sampler used by the sky shader. */
        std::uint32_t m_Sampler = 0U;

    public:
        /** Constructor: stores the bindless sampler index used by the sky shader. */
        explicit Skybox(std::uint32_t SamplerIndex);

        /** Name shown in the debug window. */
        [[nodiscard]] std::string_view Name() const override { return "Skybox (cubemap)"; }

        /** Stage in the frame graph (opaque geometry). */
        [[nodiscard]] std::string_view Phase() const override { return luvk::frame::Phases::Opaque; }

        /** Generates and uploads the 6 cubemap faces. */
        void OnAttach(luvk::Context& Ctx) override;

        /** Adds the sky pass for this frame. */
        void AddPasses(luvk::frame::FeatureContext& Frame) override;
    };
}
