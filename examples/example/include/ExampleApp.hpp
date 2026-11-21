// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <luvk/App/Application.hpp>
#include <luvk/Scene/Mesh.hpp>
#include <luvk/Scene/Texture.hpp>

namespace luvk::app
{
    class DebugDraw;
    class MeshFeature;
}

namespace Example
{
    class ExampleUi;
    class MeshShaderCube;
    class Particles;
    class RayTracingPreview;
    class ShaderPlayground;
    class Skybox;

    /**
     * The luvk gallery: every feature is an exhibit standing in one 3D scene (meshes and materials, instancing, a
     * mesh shader cube, a GPU particle fountain, a ray traced screen, a live shader screen, debug lines and a sky),
     * inspected through a single tabbed panel.
     */
    class ExampleApp final : public luvk::app::Application
    {
    public:
        /** Constructor: configures the window and the optional mesh shader / ray tracing modules. */
        ExampleApp();

        /** Destructor (defined where ExampleUi is complete). */
        ~ExampleApp() override;

    protected:
        /** Creates the exhibits, the scene content and the panel. */
        void OnInit() override;

        /** Releases the UI textures backed by feature images. */
        void OnShutdown() override;

        /** Queues the meshes and debug lines of the frame. */
        void OnUpdate(float DeltaTime) override;

        /** Draws the gallery panel. */
        void OnGui() override;

    private:
        /** Queues every mesh of the gallery (floor, pedestals, screens, spheres, instanced ring, glass). */
        void DrawMeshes();

        /** Queues the debug lines enabled in the panel. */
        void DrawDebugShapes();

        /** Procedural sky. */
        std::shared_ptr<Skybox> m_Skybox;

        /** Meshes and materials. */
        std::shared_ptr<luvk::app::MeshFeature> m_Meshes;

        /** Task + mesh shader cube. */
        std::shared_ptr<MeshShaderCube> m_Cube;

        /** GPU particle fountain. */
        std::shared_ptr<Particles> m_Fountain;

        /** Ray query Cornell box rendered into a texture. */
        std::shared_ptr<RayTracingPreview> m_RayTracing;

        /** Live Slang shader rendered into a texture. */
        std::shared_ptr<ShaderPlayground> m_ShaderLab;

        /** Debug lines. */
        std::shared_ptr<luvk::app::DebugDraw> m_DebugDraw;

        /** Tabbed panel. */
        std::unique_ptr<ExampleUi> m_Ui;

        /** Checker texture of the floor. */
        luvk::scene::Texture m_Checker;

        /** Floor plane. */
        luvk::scene::Mesh m_Floor;

        /** Unit box (pedestals, screen frames, instanced ring, glass). */
        luvk::scene::Mesh m_Box;

        /** Unit sphere. */
        luvk::scene::Mesh m_Sphere;

        /** Vertical quad with top-left texture origin (screens). */
        luvk::scene::Mesh m_Screen;

        /** Transforms of the instanced ring, rebuilt every frame. */
        std::vector<glm::mat4> m_Ring;

        /** Animation time in seconds. */
        float m_Time = 0.F;
    };
}
