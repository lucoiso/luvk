// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    /** Shader stage of a SPIR-V entry point. */
    enum class ShaderStageKind : std::uint8_t
    {
        Unknown,
        Vertex,
        Fragment,
        Compute,
        Task,
        Mesh,
        RayGeneration,
        Other,
    };

    /** One member of a reflected push-constant block (names exist when the SPIR-V keeps debug names). */
    struct PushMember
    {
        /** Member name, empty without debug names. */
        std::string Name;

        /** Byte offset inside the block. */
        std::uint32_t Offset = 0U;

        /** Byte size of the member. */
        std::uint32_t Size = 0U;
    };

    /** What luvk reads from a SPIR-V module for one entry point, without any shader compiler. */
    struct ShaderReflection
    {
        /** Entry point name. */
        std::string EntryPoint;

        /** Stage of the entry point. */
        ShaderStageKind Stage = ShaderStageKind::Unknown;

        /** Workgroup size ([numthreads] / local_size) of compute, task and mesh entry points; {1, 1, 1} otherwise. */
        std::array<std::uint32_t, 3> ThreadGroupSize = {1U, 1U, 1U};

        /** Size in bytes of the push-constant block the entry point uses (0 = none). */
        std::uint32_t PushConstantSize = 0U;

        /** Members of the push-constant block, ordered by offset. */
        std::vector<PushMember> PushMembers;
    };

    /**
     * Reflects one entry point of a SPIR-V module (SPIRV-Reflect). Throws std::invalid_argument when the code is not valid
     * SPIR-V or the entry point does not exist.
     */
    [[nodiscard]] LUVK_API ShaderReflection ReflectShader(std::span<const std::uint32_t> Code, std::string_view EntryPoint);
}
