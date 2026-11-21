// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <glm/vec2.hpp>
#include "luvk/Frame/Feature.hpp"
#include "luvk/Frame/Slots.hpp"

namespace luvk
{
    class Context;
    class Sampler;
}

namespace luvk::frame
{
    /** Per-frame inputs of SceneRenderer::AddPasses. */
    struct SceneFrame
    {
        /** Frame graph. */
        RenderGraph& Graph;

        /** Per-frame CPU memory. */
        FrameAllocator& Allocator;

        /** Output image (Slots::Backbuffer); may be invalid for headless frames. */
        TextureHandle Backbuffer = {};

        /** Output resolution. */
        Extent2D OutputExtent = {};

        /** Rectangle of the output the scene is composited into (empty = whole output). */
        Rect2D Viewport = {};

        /** Output format. */
        Format OutputFormat = Format::Undefined;

        /** Camera of the frame (Slots::View), or nullptr. */
        const ViewData* View = nullptr;

        /** Frame delta time in seconds. */
        float DeltaTime = 0.F;

        /** Subpixel projection jitter in render pixels. */
        glm::vec2 ProjectionJitter{0.F, 0.F};

        /** Total elapsed time in seconds. */
        double Time = 0.0;

        /** Number of the frame. */
        std::uint64_t FrameNumber = 0U;

        /** Async compute graph of the frame, or nullptr. */
        RenderGraph* AsyncGraph = nullptr;
    };

    /**
     * Scene half of an application: owns the FeatureStack, the frame settings (render scale, wireframe, reverse depth) and
     * the blackboard, and builds the scene part of every frame:
     *
     *   Slots::SceneColor / Slots::SceneDepth (render resolution)
     *     -> features Prepare .. Overlay (SceneClearFeature first by default) -> custom passes
     *     -> Output features (CompositeFeature by default) -> Slots::Backbuffer
     *
     * The clear and the composite are ordinary features: remove or replace them through Features(). It knows nothing about
     * windows, input or the UI.
     */
    class SceneRenderer
    {
    public:
        /** Constructor: adds the default SceneClearFeature and CompositeFeature (LinearSampler must outlive the renderer). */
        SceneRenderer(Context& Ctx, const Sampler& LinearSampler, float RenderScale);

        /** Destructor; detaches every feature. */
        ~SceneRenderer();

        /** Non-copyable. */
        SceneRenderer(const SceneRenderer&) = delete;

        /** Non-copyable. */
        SceneRenderer& operator=(const SceneRenderer&) = delete;

        /** The feature stack. */
        [[nodiscard]] FeatureStack& Features() noexcept { return m_Features; }

        /** Settings of the last built frame. */
        [[nodiscard]] const FrameSettings& Settings() const noexcept { return m_Settings; }

        /** Camera of the last built frame. */
        [[nodiscard]] const ViewData& View() const noexcept { return m_View; }

        /** Render resolution relative to the viewport. */
        [[nodiscard]] float RenderScale() const noexcept { return m_RenderScale; }

        /** Sets the render resolution relative to the viewport (clamped to [0.25, 2]). */
        void SetRenderScale(float Scale) noexcept;

        /** True when the device supports wireframe rasterization. */
        [[nodiscard]] bool SupportsWireframe() const noexcept { return m_SupportsWireframe; }

        /** Requests wireframe rasterization (ignored without device support). */
        void SetWireframe(bool Enabled) noexcept;

        /** Selects reversed depth (near = 1, far = 0). */
        void SetReverseDepth(bool Enabled) noexcept;

        /** Selects the formats of SceneColor and SceneDepth. */
        void SetSceneFormats(Format Color, Format Depth) noexcept;

        /** Updates every feature. */
        void Update(float DeltaTime) const;

        /** Builds the scene part of a frame; Custom runs between the Overlay and the Output phases. */
        void AddPasses(const SceneFrame& Frame, const std::function<void(FeatureContext&)>& Custom = {});

    private:
        /** Recomputes the settings and resizes the features when anything that matters to them changed. */
        void UpdateSettings(Extent2D OutputExtent, Format OutputFormat, Rect2D Viewport);

        /** Owning context. */
        Context& m_Context;

        /** Features in phase order. */
        FeatureStack m_Features;

        /** Settings of the frame being built. */
        FrameSettings m_Settings;

        /** Settings the features were last resized with. */
        std::optional<FrameSettings> m_Applied;

        /** Render resolution relative to the viewport. */
        float m_RenderScale = 1.F;

        /** True when fillModeNonSolid is enabled. */
        bool m_SupportsWireframe = false;

        /** Blackboard of the frame. */
        Blackboard m_Board;

        /** Camera of the frame. */
        ViewData m_View;
    };
}
