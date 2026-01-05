/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace luvk
{
    /**
     * Initializes the Slang global session.
     */
    LUVK_API void InitializeShaderCompiler();

    /**
     * Shuts down the Slang session.
     */
    LUVK_API void ShutdownShaderCompiler();

    /**
     * Structure holding the result of shader compilation.
     */
    struct LUVK_API CompilationResult
    {
        /** True if compilation succeeded. */
        bool Result{false};

        /** The resulting SPIR-V code (32-bit words). */
        std::vector<std::uint32_t> Data{};

        /** Error message if compilation failed. */
        std::string Error{};
    };

    /**
     * Compiles shader source to SPIR-V using Slang.
     * @param Source The shader source code.
     * @return A CompilationResult structure.
     */
    [[nodiscard]] LUVK_API CompilationResult CompileShaderSafe(std::string_view Source);

    /**
     * Compiles shader source to SPIR-V using Slang.
     * @param Source The shader source code.
     * @return The resulting SPIR-V code as a vector of 32-bit words.
     */
    [[nodiscard]] LUVK_API std::vector<std::uint32_t> CompileShader(std::string_view Source);
}
