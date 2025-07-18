// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <string_view>
#include <vector>
#include <glslang/Public/ShaderLang.h>
#include "luvk/Module.hpp"

namespace luvk
{
    LUVKMODULE_API void InitializeGlslang();
    LUVKMODULE_API void FinalizeGlslang();

    [[nodiscard]] LUVKMODULE_API std::vector<std::uint32_t> CompileGLSLToSPIRV(std::string_view Source, EShLanguage Stage, std::string_view EntryPoint = "main");
} // namespace luvk
