// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include "luvk/App/Feature.hpp"
#include <luvk/Resources/Pipeline.hpp>

namespace luvk
{
    class Sampler;
}

namespace luvk::app
{
    /**
     * Default FeatureStage::Output feature added by SceneRenderer: samples Slots::SceneColor (linear HDR, any
     * resolution) into Slots::Backbuffer with a fullscreen triangle, encoding sRGB when the backbuffer is UNORM.
     * Replace it (FeatureStack::Remove + Add) with any feature that writes Slots::Backbuffer, e.g. a tonemapper.
     */
    class CompositeFeature final : public RenderFeature
    {
    public:
        /** Constructor. Sampler must outlive the feature. */
        explicit CompositeFeature(const Sampler& LinearSampler);

        /** Feature name. */
        [[nodiscard]] std::string_view Name() const override { return "Composite"; }

        /** Runs last: it produces the output image. */
        [[nodiscard]] FeatureStage Stage() const override { return FeatureStage::Output; }

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
        VkFormat m_Format = VK_FORMAT_UNDEFINED;
    };
}
