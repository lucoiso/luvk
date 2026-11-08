// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <luvk/Core/Common.hpp>

namespace luvk
{
    /**
     * Source of the luvk Slang module (core/shaders/luvk.slang): the bindless arrays, the typed handles, luvk::Span<T> and
     * the reflection attributes. It is loaded in every compile session, so shaders simply `import luvk;`.
     */
    [[nodiscard]] LUVK_API std::string_view LuvkShaderModule() noexcept;

    /** Parameters for one Slang to SPIR-V compilation. */
    struct ShaderCompileOptions
    {
        /** Slang target profile string. */
        std::string_view Profile = "spirv_1_6";

        /** Module name used when loading in-memory sources and in diagnostic messages. */
        std::string_view ModuleName = "shader";

        /** Empty: every entry point marked with [shader("...")]. */
        std::string_view EntryPoint;

        /** Additional search paths used to resolve imports and include directives. */
        std::vector<std::string> SearchPaths;

        /** Preprocessor macros (name, value) defined for the compilation. */
        std::vector<std::pair<std::string, std::string>> Defines;

        /** Compile with high optimization when true, no optimization otherwise. */
        bool Optimize = true;

        /**
         * Stage of EntryPoint when the function has no [shader("...")] attribute: "vertex", "fragment" (or "pixel"),
         * "compute", "mesh", "task" (or "amplification"), "geometry", "hull", "domain", "raygen", "closesthit",
         * "anyhit", "miss", "intersection" or "callable". Ignored when EntryPoint is empty or carries the attribute.
         */
        std::string_view Stage;

        /**
         * Content-addressed cache: when set, results are stored there and a later compilation with the same compiler,
         * options and file contents (the source and every file it imported) is served without invoking Slang.
         */
        std::filesystem::path CacheDirectory;
    };

    /** One user attribute of a reflected field, e.g. [Range(0, 1)] or [Tooltip("...")]. */
    struct ShaderAttribute
    {
        /** Attribute name without the "Attribute" suffix ("Range", "Tooltip", "Slot", ...). */
        std::string Name;

        /** Numeric arguments in declaration order. */
        std::vector<double> Numbers;

        /** String arguments in declaration order. */
        std::vector<std::string> Strings;
    };

    /** One field of a reflected block. */
    struct ShaderField
    {
        /** Field name. */
        std::string Name;

        /** Slang type name (e.g. "float", "vector<float,3>", "Texture2D"). */
        std::string Type;

        /** Byte offset inside the block. */
        std::uint32_t Offset = 0U;

        /** Byte size. */
        std::uint32_t Size = 0U;

        /** User attributes. */
        std::vector<ShaderAttribute> Attributes;

        /** The attribute named Name, or nullptr. */
        [[nodiscard]] const ShaderAttribute* Find(std::string_view AttributeName) const noexcept;
    };

    /** A reflected constant block: the push constants or a struct reached through a pointer of the push constants. */
    struct ShaderBlock
    {
        /** "push" for the push-constant block, otherwise the name of the pointer field leading to the struct. */
        std::string Name;

        /** Struct type name. */
        std::string Type;

        /** Byte size of the struct. */
        std::uint32_t Size = 0U;

        /** Fields in declaration order. */
        std::vector<ShaderField> Fields;
    };

    /** One reflected entry point. */
    struct ShaderEntry
    {
        /** Function name. */
        std::string Name;

        /** Stage name ("compute", "vertex", ...). */
        std::string Stage;

        /** [numthreads] of compute, task and mesh entry points; {1, 1, 1} otherwise. */
        std::array<std::uint32_t, 3> ThreadGroupSize = {1U, 1U, 1U};
    };

    /** What Slang knows about a compiled program, beyond what the SPIR-V carries. */
    struct ShaderProgramReflection
    {
        /** Entry points of the program. */
        std::vector<ShaderEntry> EntryPoints;

        /** Push-constant block first, then the structs reached through its pointers. */
        std::vector<ShaderBlock> Blocks;

        /** The block named Name, or nullptr. */
        [[nodiscard]] const ShaderBlock* Find(std::string_view BlockName) const noexcept;
    };

    /** SPIR-V of a compilation, with every file it read and its reflection. */
    struct ShaderCompileResult
    {
        /** SPIR-V words. */
        std::vector<std::uint32_t> Spirv;

        /** Absolute paths of the source file and of every file it loaded (imports, includes): watch them for hot reload. */
        std::vector<std::filesystem::path> Dependencies;

        /** Entry points and blocks. */
        ShaderProgramReflection Reflection;

        /** True when the result came from ShaderCompileOptions::CacheDirectory. */
        bool FromCache = false;
    };

    /** Compiles Slang source to SPIR-V (thread-safe, one Slang global session behind a mutex). Returns the result or the diagnostics. */
    [[nodiscard]] LUVK_API std::expected<ShaderCompileResult, std::string> CompileShader(std::string_view Source, const ShaderCompileOptions& Options = {});

    /**
     * Compiles a Slang file to SPIR-V (thread-safe). The file's directory is searched first for imports and includes; the
     * module is named after the file stem unless Options.ModuleName is changed from its default.
     */
    [[nodiscard]] LUVK_API std::expected<ShaderCompileResult, std::string> CompileShaderFile(const std::filesystem::path& Path, const ShaderCompileOptions& Options = {});

    /** CompileShader that throws std::runtime_error with the diagnostics on failure and returns the SPIR-V. */
    [[nodiscard]] LUVK_API std::vector<std::uint32_t> CompileShaderOrThrow(std::string_view Source, const ShaderCompileOptions& Options = {});

    /** Shorthand compiling one entry point of a named module, e.g. CompileShaderOrThrow(Source, "particles", "ComputeMain"). */
    [[nodiscard]] inline std::vector<std::uint32_t> CompileShaderOrThrow(const std::string_view Source, const std::string_view ModuleName, const std::string_view EntryPoint)
    {
        return CompileShaderOrThrow(Source, ShaderCompileOptions{.ModuleName = ModuleName, .EntryPoint = EntryPoint});
    }

    /**
     * Polls a set of files on the caller's thread at a fixed rate and reports the ones whose write time changed (hot
     * reload). Nothing runs in the background and nothing is allocated once the set is built.
     */
    class LUVK_API FileWatcher
    {
        /** One watched file. */
        struct Entry
        {
            /** Watched path. */
            std::filesystem::path Path;

            /** Last observed write time. */
            std::filesystem::file_time_type WriteTime;
        };

        /** Watched files. */
        std::vector<Entry> m_Files;

        /** Files reported by the last Poll. */
        std::vector<std::filesystem::path> m_Changed;

        /** Minimum time between two scans. */
        std::chrono::steady_clock::duration m_Interval;

        /** Time of the last scan. */
        std::chrono::steady_clock::time_point m_LastScan;

    public:
        /** Constructor; Interval is the minimum time between two scans (default 500 ms, i.e. 2 Hz). */
        explicit FileWatcher(std::chrono::steady_clock::duration Interval = std::chrono::milliseconds(500));

        /** Starts watching Path (no-op when already watched). */
        void Watch(const std::filesystem::path& Path);

        /** Stops watching every file. */
        void Clear() noexcept;

        /** Number of watched files. */
        [[nodiscard]] std::size_t Count() const noexcept { return m_Files.size(); }

        /** Files changed since the previous scan; empty when the interval has not elapsed. Valid until the next Poll. */
        [[nodiscard]] const std::vector<std::filesystem::path>& Poll();
    };
}
