// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#ifdef LUVK_SLANG_INCLUDED

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace luvk
{
    LUVK_API void InitializeShaderCompiler();
    LUVK_API void ShutdownShaderCompiler();

    struct LUVK_API CompilationResult
    {
        bool                       Result{false};
        std::vector<std::uint32_t> Data{};
        std::string                Error{};
    };

    [[nodiscard]] LUVK_API CompilationResult          CompileShaderSafe(std::string_view Source, std::string_view Profile = "spirv_1_0");
    [[nodiscard]] LUVK_API std::vector<std::uint32_t> CompileShader(std::string_view Source, std::string_view Profile = "spirv_1_0");
} // namespace luvk

#endif // LUVK_SLANG_INCLUDED
