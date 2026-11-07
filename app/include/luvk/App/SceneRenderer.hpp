// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include "luvk/App/Feature.hpp"
#include "luvk/App/Scene.hpp"

namespace luvk
{
    class Context;
    class Sampler;
}

namespace luvk::app
{
    /** Per-frame inputs of SceneRenderer::AddPasses. */
    struct SceneFrame
    {
        /** Frame graph of the current frame. */
        RenderGraph& Graph;

        /** Per-frame memory of the current frame. */
        FrameAllocator& Allocator;

        /** Swapchain image the Output feature writes (published as Slots::Backbuffer). */
        TextureHandle Backbuffer = {};

        /** Backbuffer extent in pixels. */
        VkExtent2D OutputExtent = {};

        /** Backbuffer format. */
        VkFormat OutputFormat = VK_FORMAT_UNDEFINED;

        /** Camera of the frame, published as Slots::View; nullptr when the application has no camera. */
        const ViewData* View = nullptr;

        /** Frame delta time in seconds. */
        float DeltaTime = 0.F;

        /** Total elapsed time in seconds. */
        double Time = 0.0;

        /** Number of the current frame. */
        std::uint64_t FrameNumber = 0U;
    };

    /**
     * Scene half of the application: owns the FeatureStack, the frame settings (render scale, wireframe) and the
     * blackboard, and builds the scene part of every frame:
     *
     *   Slots::SceneColor / Slots::SceneDepth (render resolution, cleared)
     *     -> features Prepare .. Overlay -> custom passes -> Output features (CompositeFeature by default)
     *     -> Slots::Backbuffer
     *
     * It knows nothing about windows, input or the UI; Application wires it to the Renderer.
     */
    class SceneRenderer
    {
    public:
        /** Constructor. Adds the default CompositeFeature (LinearSampler must outlive the scene). */
        SceneRenderer(Context& Ctx, const Sampler& LinearSampler, float RenderScale);

        /** Destructor. Detaches every feature. */
        ~SceneRenderer();

        /** Non-copyable. */
        SceneRenderer(const SceneRenderer&) = delete;

        /** Non-copyable. */
        SceneRenderer& operator=(const SceneRenderer&) = delete;

        /** Feature stack executed every frame. */
        [[nodiscard]] FeatureStack& Features() noexcept { return m_Features; }

        /** Settings of the last frame (extents, formats, wireframe). */
        [[nodiscard]] const FrameSettings& Settings() const noexcept { return m_Settings; }

        /** Camera of the last frame (identity when the application has no camera). */
        [[nodiscard]] const ViewData& View() const noexcept { return m_View; }

        /** Scene resolution relative to the output, clamped to [0.25, 2.0]. */
        [[nodiscard]] float RenderScale() const noexcept { return m_RenderScale; }

        /** Sets the scene resolution relative to the output (clamped to [0.25, 2.0]); applies on the next frame. */
        void SetRenderScale(float Scale) noexcept;

        /** True when the device supports wireframe rasterization (fillModeNonSolid). */
        [[nodiscard]] bool SupportsWireframe() const noexcept { return m_SupportsWireframe; }

        /** Requests wireframe rasterization; ignored when unsupported, so FrameSettings::Wireframe is always valid. */
        void SetWireframe(bool Enabled) noexcept;

        /** Switches FrameSettings::ReverseDepth; features are resized (and rebuild their pipelines) on the next frame. */
        void SetReverseDepth(bool Enabled) noexcept;

        /** Calls Update on every enabled feature. */
        void Update(float DeltaTime) const;

        /** Builds the scene part of the frame; Custom adds passes after the Overlay stage and before the Output one. */
        void AddPasses(const SceneFrame& Frame, const std::function<void(FeatureContext&)>& Custom = {});

    private:
        /** Recomputes the settings for the output and notifies the features when anything changed. */
        void UpdateSettings(VkExtent2D OutputExtent, VkFormat OutputFormat);

        /** Owning context. */
        Context& m_Context;

        /** Features executed every frame. */
        FeatureStack m_Features;

        /** Settings of the current frame. */
        FrameSettings m_Settings;

        /** Settings the features were last resized with. */
        std::optional<FrameSettings> m_Applied;

        /** Scene resolution relative to the output. */
        float m_RenderScale = 1.F;

        /** True when the device supports fillModeNonSolid. */
        bool m_SupportsWireframe = false;

        /** Frame blackboard (graph handles and values shared by the features). */
        Blackboard m_Board;

        /** Camera of the last frame. */
        ViewData m_View;
    };
}
