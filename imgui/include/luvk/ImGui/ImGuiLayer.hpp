// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <imgui.h>
#include <luvk/Render/RenderGraph.hpp>

struct SDL_Window;
union SDL_Event;

namespace luvk
{
    class Context;
    class FrameAllocator;
    class Image;
    class Renderer;
}

namespace luvk::platform
{
    class Window;
}

namespace luvk::imgui
{
    class ImGuiRenderer;
    struct ImGuiGeometry;

    /**
     * Dear ImGui host: context, SDL3 platform backend and the luvk-based renderer backend.
     *
     * Per frame: NewFrame -> (UI code) -> Render, then, inside a Renderer frame: PrepareFrame -> AddRenderPass
     * (main window) -> AddViewportPasses (secondary OS windows, presented in the same submission).
     * The ImGui layout file (Options::IniPath) is owned here, so its path outlives the ImGui context.
     */
    class ImGuiLayer
    {
    public:
        /** UI options. */
        struct Options
        {
            /** Enables docking. */
            bool Docking = true;

            /** Enables multi-viewport (secondary OS windows). */
            bool Viewports = true;

            /** Layout persistence file; empty disables saving and loading the layout. */
            std::filesystem::path IniPath = "imgui.ini";

            /** Clear color of secondary viewport windows. */
            std::array<float, 4> ViewportClearColor = {0.02F, 0.02F, 0.03F, 1.F};
        };

        /** Constructor. Creates the ImGui context and backends. */
        ImGuiLayer(Context& Ctx, luvk::Renderer& TargetRenderer, platform::Window& Window, const Options& Options);

        /** Destructor. Destroys the platform windows, the renderer backend and the context (saving the layout). */
        ~ImGuiLayer();

        /** Non-copyable. */
        ImGuiLayer(const ImGuiLayer&) = delete;

        /** Non-copyable. */
        ImGuiLayer& operator=(const ImGuiLayer&) = delete;

        /** Forwards one SDL event to the ImGui platform backend (main and secondary windows). */
        void ProcessEvent(const SDL_Event& Event);

        /** Starts a new ImGui frame (platform new-frame + ImGui::NewFrame). */
        void NewFrame();

        /** Ends the ImGui frame: builds the draw data and updates the secondary OS windows. */
        void Render();

        /** Uploads the frame's UI textures and copies the geometry of every viewport into Allocator. Call after Renderer::BeginFrame. */
        void PrepareFrame(FrameAllocator& Allocator);

        /** Adds the main viewport's ImGui pass to the frame graph, rendering into Backbuffer. */
        void AddRenderPass(RenderGraph& Graph,
                           TextureHandle Backbuffer,
                           VkFormat Format,
                           LoadOp Load,
                           VkClearColorValue Clear = {});

        /** Acquires every visible secondary viewport window through the Renderer and adds its ImGui pass. */
        void AddViewportPasses(RenderGraph& Graph);

        /** Registers a user image and returns the ImTextureID to pass to ImGui::Image (idempotent). */
        [[nodiscard]] ImTextureID AddTexture(Image& Texture);

        /** Unregisters a user image previously passed to AddTexture. */
        void RemoveTexture(const Image& Texture);

        /** Content scale of the main window's display (1.0 at 100%, 1.5 at 150%...). */
        [[nodiscard]] float DisplayScale() const noexcept;

        /** True when the context and backends were initialized. */
        [[nodiscard]] bool IsInitialized() const noexcept { return m_Initialized; }

    private:
        /** Imports every image the UI samples and declares it as sampled by Pass. */
        void UseLiveImages(RenderGraph& Graph, PassBuilder& Pass) const;

        /** Owning context. */
        Context* m_Context;

        /** Owning renderer. */
        Renderer* m_Renderer;

        /** Main window, used by the platform backend. */
        SDL_Window* m_Window;

        /** UI options. */
        Options m_Options;

        /** Storage behind ImGuiIO::IniFilename (Dear ImGui keeps the pointer, not a copy). */
        std::string m_IniPath;

        /** luvk renderer backend. */
        std::unique_ptr<ImGuiRenderer> m_Backend;

        /** Main viewport geometry of the frame being recorded. */
        std::unique_ptr<ImGuiGeometry> m_Geometry;

        /** True once context, platform backend and renderer backend are ready. */
        bool m_Initialized = false;
    };
}
