// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <string_view>
#include <glslang/Public/ShaderLang.h>
#include "luvk/Module.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    LUVKMODULE_API void InitializeGlslang();
    LUVKMODULE_API void FinalizeGlslang();

    [[nodiscard]] LUVKMODULE_API Vector<std::uint32_t> CompileGLSLToSPIRV(const std::string_view& Source, EShLanguage Stage, const std::string_view& EntryPoint = "main");
} // namespace luvk
