// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include "luvk/Module.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    LUVKMODULE_API void InitializeShaderCompiler();
    LUVKMODULE_API void ShutdownShaderCompiler();

    struct LUVKMODULE_API CompilationResult
    {
        bool                  Result{false};
        Vector<std::uint32_t> Data{};
        std::string           Error{};
    };

    [[nodiscard]] LUVKMODULE_API CompilationResult     CompileShaderSafe(const std::string_view& Source);
    [[nodiscard]] LUVKMODULE_API Vector<std::uint32_t> CompileShader(const std::string_view& Source);
} // namespace luvk
