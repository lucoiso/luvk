// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <luvk/Frame/Feature.hpp>
#include <luvk/Resources/Image.hpp>
#include <luvk/Resources/Pipeline.hpp>

namespace Example
{
    /** Live Slang editor: the fragment shader is recompiled at runtime and renders into a preview image. Replacing the pipeline needs no device wait: the old one is retired through the deletion queue. */
    class ShaderPlayground final : public luvk::frame::RenderFeature
    {
    private:
        /** Preview image the compiled fragment shader renders into. */
        luvk::Image m_Output;

        /** Pipeline of the last successful compilation. */
        luvk::Pipeline m_Pipeline;

        /** Context used to create pipelines (set on attach). */
        luvk::Context* m_Context = nullptr;

        /** Compiled vertex shader shared by every playground pipeline. */
        std::vector<std::uint32_t> m_VertexShader;

        /** Time pushed to the shader every frame (seconds), reset after each compilation. */
        float m_Time = 0.F;

    public:
        /** Name shown in the debug window. */
        [[nodiscard]] std::string_view Name() const override { return "Shader Playground"; }

        /** Stage in the frame graph (offscreen preparation). */
        [[nodiscard]] std::string_view Phase() const override { return luvk::frame::Phases::Prepare; }

        /** Creates the preview image and compiles the default fragment shader. */
        void OnAttach(luvk::Context& Ctx) override;

        /** Advances the shader time by DeltaTime seconds. */
        void Update(float DeltaTime) override;

        /** Adds the preview pass for this frame. */
        void AddPasses(luvk::frame::FeatureContext& Frame) override;

        /** Compiles a fragment source and swaps the pipeline: returns an empty string on success, the compiler diagnostics otherwise. */
        std::string Compile(std::string_view FragmentSource);

        /** Default fragment source shown in the editor on startup. */
        [[nodiscard]] static std::string_view DefaultSource();

        /** Preview image the UI can display. */
        [[nodiscard]] luvk::Image& Output() noexcept { return m_Output; }
    };
}
