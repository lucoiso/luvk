// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <luvk/Resources/Pipeline.hpp>
#include "luvk/Frame/Feature.hpp"

namespace luvk
{
    class Sampler;
}

namespace luvk::frame
{
    /** Default Phases::Prepare feature added by SceneRenderer: clears SceneColor and SceneDepth. Remove it when another
     *  feature writes every pixel of both. */
    class SceneClearFeature final : public RenderFeature
    {
    public:
        /** Color SceneColor is cleared to. */
        std::array<float, 4> Color = {0.F, 0.F, 0.F, 1.F};

        /** Feature name. */
        [[nodiscard]] std::string_view Name() const override { return "luvk.SceneClear"; }

        /** Runs first: SceneRenderer adds it before any other feature, and ties keep the insertion order. */
        [[nodiscard]] std::string_view Phase() const override { return Phases::Prepare; }

        /** Adds the clear pass. */
        void AddPasses(FeatureContext& Frame) override;
    };

    /**
     * Default Phases::Output feature added by SceneRenderer: samples Slots::SceneColor (linear HDR, any resolution) into
     * Slots::Backbuffer with a fullscreen triangle, encoding sRGB when the backbuffer is UNORM. Replace it with any feature
     * that writes Slots::Backbuffer, e.g. a tonemapper.
     */
    class CompositeFeature final : public RenderFeature
    {
    public:
        /** Constructor. Sampler must outlive the feature. */
        explicit CompositeFeature(const Sampler& LinearSampler);

        /** Feature name. */
        [[nodiscard]] std::string_view Name() const override { return "luvk.Composite"; }

        /** Runs last: it produces the output image. */
        [[nodiscard]] std::string_view Phase() const override { return Phases::Output; }

        /** Keeps the context used to build the pipeline. */
        void OnAttach(Context& Ctx) override;

        /** Releases the pipeline. */
        void OnDetach(Context& Ctx) override;

        /** Adds the composite pass (rebuilding the pipeline when the output format changes). */
        void AddPasses(FeatureContext& Frame) override;

    private:
        /** Owning context. */
        Context* m_Context = nullptr;

        /** Linear clamp sampler. */
        const Sampler* m_Sampler;

        /** Composite pipeline. */
        Pipeline m_Pipeline;

        /** Output format the pipeline was built for. */
        Format m_Format = Format::Undefined;
    };
}
