// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** ShaderCompiler.hpp definitions */

#include "luvk/Module.hpp"

#include <glslang/Public/ShaderLang.h>
#include <span>
#include <string_view>
#include <vector>
#include <cstdint>

namespace luvk
{
    /** Initialize the glslang compiler process */
    LUVKMODULE_API void InitializeGlslang();

    /** Finalize the glslang compiler process */
    LUVKMODULE_API void FinalizeGlslang();

    /** Compile GLSL source code to SPIR-V */
    [[nodiscard]] LUVKMODULE_API std::vector<std::uint32_t> CompileGLSLToSPIRV(std::string_view Source, EShLanguage Stage, std::string_view EntryPoint = "main");
} // namespace luvk
