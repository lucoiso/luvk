// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <optional>
#include <string>
#include <glm/glm.hpp>
#include <imgui.h>

namespace luvk::app
{
    class Application;
    class DebugDraw;
    class MeshFeature;
    class RenderFeature;
}

namespace Example
{
    class MeshShaderCube;
    class Particles;
    class RayTracingPreview;
    class ShaderPlayground;
    class Skybox;

    /** The gallery's features, as the UI needs them. */
    struct GalleryExhibits
    {
        /** Procedural sky. */
        Skybox& Sky;

        /** Meshes and materials (floor, screens, spheres, instancing, glass). */
        luvk::app::MeshFeature& Meshes;

        /** Task + mesh shader cube. */
        MeshShaderCube& Cube;

        /** GPU particle fountain. */
        Particles& Fountain;

        /** Ray query Cornell box. */
        RayTracingPreview& RayTracing;

        /** Live Slang shader. */
        ShaderPlayground& ShaderLab;

        /** Debug lines. */
        luvk::app::DebugDraw& Debug;
    };

    /**
     * The gallery's single panel, with one tab per topic: Scene (exhibits, debug draw, lighting), Ray Tracing,
     * Shader Lab (live editor) and Diagnostics (luvk statistics, device capabilities, multi-viewport demo).
     */
    class ExampleUi
    {
    public:
        /** Constructor. App and Exhibits must outlive the UI. */
        ExampleUi(luvk::app::Application& App, const GalleryExhibits& Exhibits);

        /** Draws the panel and the optional extra windows. */
        void Draw();

        /** Whether the floor grid is drawn. */
        [[nodiscard]] bool DrawGrid() const noexcept { return m_DrawGrid; }

        /** Whether the world axes are drawn. */
        [[nodiscard]] bool DrawAxes() const noexcept { return m_DrawAxes; }

        /** Whether the mesh shader cube bounds are drawn. */
        [[nodiscard]] bool DrawBounds() const noexcept { return m_DrawBounds; }

        /** Inverse view-projection of the frozen camera frustum, when frozen. */
        [[nodiscard]] const std::optional<glm::mat4>& FrozenFrustum() const noexcept { return m_FrozenFrustum; }

    private:
        /** Exhibits, debug draw toggles and lighting. */
        void DrawSceneTab();

        /** Ray traced image, description and controls. */
        void DrawRayTracingTab();

        /** Live shader editor and its preview. */
        void DrawShaderLabTab();

        /** luvk statistics, device capabilities and window demos. */
        void DrawDiagnosticsTab();

        /** One exhibit row: enable toggle, "go to" button and the feature's own controls. */
        void DrawExhibit(luvk::app::RenderFeature& Feature, const char* Label, const std::optional<glm::vec3>& Focus);

        /** A second OS window demonstrating Dear ImGui multi-viewport. */
        void DrawFloatingWindow();

        /** Moves the camera in front of Focus. */
        void Focus(const glm::vec3& Target) const;

        /** Draws an image scaled to fit the available width (kept square). */
        static void ImageFit(ImTextureID Texture, float MaxSide);

        /** Application (camera, diagnostics, UI textures). */
        luvk::app::Application& m_App;

        /** Gallery features. */
        GalleryExhibits m_Exhibits;

        /** Shader Lab editor contents. */
        std::string m_ShaderCode;

        /** Last Shader Lab compilation diagnostics (empty on success). */
        std::string m_CompileError;

        /** Frozen camera frustum, drawn with debug lines. */
        std::optional<glm::mat4> m_FrozenFrustum;

        /** Floor grid toggle. */
        bool m_DrawGrid = false;

        /** World axes toggle. */
        bool m_DrawAxes = true;

        /** Cube bounds toggle. */
        bool m_DrawBounds = true;

        /** Multi-viewport demo window toggle. */
        bool m_ShowFloatingWindow = false;

        /** Whether the floating window was already placed outside the main window. */
        bool m_FloatingWindowPlaced = false;

        /** Dear ImGui demo window toggle. */
        bool m_ShowDemoWindow = false;
    };
}
