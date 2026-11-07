// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <glm/glm.hpp>
#include <luvk/Scene/MeshRenderer.hpp>
#include "luvk/App/Feature.hpp"

namespace luvk::app
{
    /**
     * Opaque-stage feature drawing luvk::scene meshes into the scene color/depth with the frame's camera
     * (available when luvk-scene is built). Queue draws from OnUpdate; they are recorded with the frame.
     *
     *     auto Meshes = Features().Add<luvk::app::MeshFeature>();
     *     Meshes->Draw(Cube, Material, Transform);
     */
    class MeshFeature final : public RenderFeature
    {
    public:
        /** Feature name. */
        [[nodiscard]] std::string_view Name() const override { return "Meshes"; }

        /** Draws with the opaque geometry (transparent materials are ordered after the opaque ones inside the pass). */
        [[nodiscard]] FeatureStage Stage() const override { return FeatureStage::Opaque; }

        /** Creates the mesh renderer. */
        void OnAttach(Context& Ctx) override;

        /** Releases the mesh renderer. */
        void OnDetach(Context& Ctx) override;

        /** Records the queued draws (needs Slots::View, i.e. an application camera). */
        void AddPasses(FeatureContext& Frame) override;

        /** Draw statistics and light controls. */
        void OnGui() override;

        /** Queues one instance of Target. */
        void Draw(const scene::Mesh& Target, const scene::Material& Surface, const glm::mat4& Transform);

        /** Queues one instance of Target per transform. */
        void DrawInstanced(const scene::Mesh& Target, const scene::Material& Surface, std::span<const glm::mat4> Transforms);

        /** The underlying renderer (light, statistics). */
        [[nodiscard]] scene::MeshRenderer& Renderer() noexcept { return *m_Renderer; }

    private:
        /** Mesh renderer created on attach. */
        std::unique_ptr<scene::MeshRenderer> m_Renderer;
    };
}
