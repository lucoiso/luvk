// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <imgui.h>
#include <luvk/Core/Common.hpp>
#include <luvk/Render/FrameAllocator.hpp>
#include <luvk/Resources/Pipeline.hpp>
#include <luvk/Resources/Sampler.hpp>

namespace luvk
{
    class CommandList;
    class Context;
    class Image;
}

namespace luvk::imgui
{
    /** Geometry of one viewport for the frame being recorded: ranges of the renderer's FrameAllocator. */
    struct ImGuiGeometry
    {
        /** Every draw list's ImDrawVert data, back to back. */
        FrameAllocation Vertices;

        /** Every draw list's ImDrawIdx data, back to back. */
        FrameAllocation Indices;
    };

    /**
     * Dear ImGui renderer backend built on luvk primitives (Pipeline, Buffer, Image, Sampler, CommandList and the
     * bindless heap). There are no descriptor sets or Vulkan render passes here: the backend draws with dynamic
     * rendering and pulls its font/user textures from the global bindless set. Secondary viewports only get their
     * window state here (ImGuiViewportWindow); ImGuiLayer renders them inside the main frame.
     */
    class ImGuiRenderer
    {
    public:
        /** Constructor. Creates the shared sampler and fallback texture and registers the viewport callbacks. */
        ImGuiRenderer(Context& Ctx, std::uint32_t FramesInFlight, bool EnableViewports);

        /** Destructor. */
        ~ImGuiRenderer();

        /** Non-copyable. */
        ImGuiRenderer(const ImGuiRenderer&) = delete;

        /** Non-copyable. */
        ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;

        /** Creates/updates/destroys every texture Dear ImGui requested this frame. */
        void UpdateTextures();

        /** Copies one frame of draw data into per-frame memory (empty geometry when there is nothing to draw). */
        [[nodiscard]] ImGuiGeometry UploadDrawData(const ImDrawData& DrawData, FrameAllocator& Allocator) const;

        /** Records DrawData into the current pass, rendering to TargetExtent with the pipeline for Format. */
        void RenderDrawData(CommandList& Cmd, const ImDrawData& DrawData, const ImGuiGeometry& Geometry, Extent2D TargetExtent, Format Format);

        /** Every image the current UI samples (backend textures plus user images registered with AddTexture). */
        [[nodiscard]] const std::vector<Image*>& LiveImages() const noexcept { return m_LiveImages; }

        /** Registers a user image with SAMPLED usage and returns its ImTextureID (idempotent). */
        [[nodiscard]] ImTextureID AddTexture(Image& Texture);

        /** Unregisters a user image previously registered with AddTexture. */
        void RemoveTexture(const Image& Texture);

        /** Creates the window state (surface + present target) of a secondary viewport. */
        void CreateViewportWindow(ImGuiViewport* Viewport);

        /** Destroys the window state of a secondary viewport. */
        void DestroyViewportWindow(ImGuiViewport* Viewport);

    private:
        /** One texture owned by the backend (font atlas or any texture Dear ImGui manages itself). */
        struct TextureRecord
        {
            /** GPU image containing the pixels. */
            std::unique_ptr<Image> Texture;
        };

        /** Returns (creating when missing) the pipeline that renders into Format. */
        Pipeline& PipelineForFormat(Format Format);

        /** Push constants of ImGui.slang (vertices are pulled from the frame allocator). */
        struct PushConstants
        {
            /** Scale applied to pixel coordinates. */
            float Scale[2] = {0.F, 0.F};

            /** Translation applied after scaling. */
            float Translate[2] = {0.F, 0.F};

            /** Device address of the frame's ImDrawVert array. */
            DeviceAddress Vertices = 0U;

            /** Bindless sampled texture. */
            Texture2D Texture;

            /** Bindless sampler. */
            SamplerHandle Sampler;
        };

        /** Rebuilds the immutable list of live images (backend + user textures). */
        void RefreshLiveImages();

        /** Owning context. */
        Context& m_Context;

        /** Frames in flight of the renderer (acquire semaphores of the viewport windows). */
        std::uint32_t m_FrameCount;

        /** True when multi-viewport support was requested. */
        bool m_Viewports;

        /** Shared linear clamp sampler. */
        Sampler m_Sampler;

        /** 1x1 white texture used when a draw command has no valid texture. */
        std::unique_ptr<Image> m_WhiteTexture;

        /** Bindless index of the fallback texture. */
        std::uint32_t m_WhiteIndex = 0U;

        /** One pipeline per color format (main window and secondary viewports may differ). */
        std::unordered_map<Format, std::unique_ptr<Pipeline>> m_Pipelines;

        /** Textures created and updated by Dear ImGui itself. */
        std::unordered_map<ImTextureData*, TextureRecord> m_BackendTextures;

        /** User images exposed through AddTexture (not owned). */
        std::unordered_set<Image*> m_UserTextures;

        /** Cached list of images sampled by the current UI. */
        std::vector<Image*> m_LiveImages;

        /** Bindless indices of every image that may be referenced by a draw command. */
        std::unordered_set<std::uint32_t> m_ValidIndices;
    };
}
