// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <expected>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    /**
     * Slang module declaring the luvk bindless interface. It is pre-loaded in every compile session, so
     * shaders can simply `import luvk;` and use:
     *   luvk::Textures[i] (Texture2D), luvk::TextureCubes[i] (TextureCube), luvk::Samplers[i] (SamplerState),
     *   luvk::Images[i] (RWTexture2D<float4>). Textures and TextureCubes share binding 0 (cube-compatible
     * views). Acceleration structures (RayTracingModule) live at binding 3 and are declared by the shaders
     * that use them, since their type requires ray query support.
     */
    inline constexpr std::string_view BindlessShaderModule = R"slang(
module luvk;

namespace luvk
{
    [[vk::binding(0, 0)]] public Texture2D           Textures[];
    [[vk::binding(0, 0)]] public TextureCube         TextureCubes[];
    [[vk::binding(1, 0)]] public SamplerState        Samplers[];
    [[vk::binding(2, 0)]] public RWTexture2D<float4> Images[];

    public float4 Sample(uint Texture, uint Sampler, float2 UV)
    {
        return Textures[NonUniformResourceIndex(Texture)].Sample(Samplers[NonUniformResourceIndex(Sampler)], UV);
    }

    public float4 SampleCube(uint Texture, uint Sampler, float3 Direction)
    {
        return TextureCubes[NonUniformResourceIndex(Texture)].Sample(Samplers[NonUniformResourceIndex(Sampler)], Direction);
    }
}
)slang";

    /** Parameters for one Slang to SPIR-V compile session. */
    struct ShaderCompileOptions
    {
        /** Slang target profile string (default "spirv_1_6"). */
        std::string_view Profile = "spirv_1_6";

        /** Module name used when loading the source and in diagnostic messages. */
        std::string_view ModuleName = "shader";

        /** Empty: every entry point marked with [shader("...")]. */
        std::string_view EntryPoint;

        /** Additional search paths used to resolve imports and include directives. */
        std::vector<std::string> SearchPaths;

        /** Preprocessor macros (name, value) defined for the compile session. */
        std::vector<std::pair<std::string, std::string>> Defines;

        /** Compile with high optimization when true, no optimization otherwise. */
        bool Optimize = true;
    };

    /** Compiles Slang source to SPIR-V: thread-safe, with the Slang global session created on first use. Returns the SPIR-V words or the compiler diagnostics. */
    [[nodiscard]] LUVK_API std::expected<std::vector<std::uint32_t>, std::string> CompileShader(std::string_view Source, const ShaderCompileOptions& Options = {});

    /** CompileShader that throws std::runtime_error with the diagnostics on failure. */
    [[nodiscard]] LUVK_API std::vector<std::uint32_t> CompileShaderOrThrow(std::string_view Source, const ShaderCompileOptions& Options = {});

    /** Shorthand compiling one entry point of a named module, e.g. CompileShaderOrThrow(Source, "particles", "ComputeMain"). */
    [[nodiscard]] inline std::vector<std::uint32_t> CompileShaderOrThrow(const std::string_view Source, const std::string_view ModuleName, const std::string_view EntryPoint)
    {
        return CompileShaderOrThrow(Source, ShaderCompileOptions{.ModuleName = ModuleName, .EntryPoint = EntryPoint});
    }
}
