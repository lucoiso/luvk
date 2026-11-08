// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/ShaderReflection.hpp"
#include <algorithm>
#include <stdexcept>
#include <spirv_reflect.h>

namespace
{
    /** Owns a SpvReflectShaderModule for the duration of a reflection. */
    struct ModuleGuard
    {
        SpvReflectShaderModule Module = {};

        ~ModuleGuard()
        {
            spvReflectDestroyShaderModule(&Module);
        }
    };

    [[nodiscard]] luvk::ShaderStageKind ToStage(const SpvReflectShaderStageFlagBits Stage) noexcept
    {
        switch (Stage)
        {
        case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT: return luvk::ShaderStageKind::Vertex;
        case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT: return luvk::ShaderStageKind::Fragment;
        case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT: return luvk::ShaderStageKind::Compute;
        case SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT: return luvk::ShaderStageKind::Task;
        case SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT: return luvk::ShaderStageKind::Mesh;
        case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR: return luvk::ShaderStageKind::RayGeneration;
        default: return luvk::ShaderStageKind::Other;
        }
    }
}

luvk::ShaderReflection luvk::ReflectShader(const std::span<const std::uint32_t> Code, const std::string_view EntryPoint)
{
    ModuleGuard Guard;
    if (spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NO_COPY, Code.size_bytes(), std::data(Code), &Guard.Module) != SPV_REFLECT_RESULT_SUCCESS)
    {
        throw std::invalid_argument("[luvk] ReflectShader: the code is not valid SPIR-V.");
    }

    const std::string Name(EntryPoint);
    const SpvReflectEntryPoint* const Entry = spvReflectGetEntryPoint(&Guard.Module, Name.c_str());
    if (Entry == nullptr)
    {
        throw std::invalid_argument("[luvk] ReflectShader: the SPIR-V has no entry point '" + Name + "'.");
    }

    ShaderReflection Result{.EntryPoint = Name, .Stage = ToStage(Entry->shader_stage)};
    if (Result.Stage == ShaderStageKind::Compute || Result.Stage == ShaderStageKind::Task || Result.Stage == ShaderStageKind::Mesh)
    {
        Result.ThreadGroupSize = {std::max(Entry->local_size.x, 1U), std::max(Entry->local_size.y, 1U), std::max(Entry->local_size.z, 1U)};
    }

    std::uint32_t Count = 0U;
    if (spvReflectEnumerateEntryPointPushConstantBlocks(&Guard.Module, Name.c_str(), &Count, nullptr) != SPV_REFLECT_RESULT_SUCCESS || Count == 0U)
    {
        return Result;
    }

    std::vector<SpvReflectBlockVariable*> Blocks(Count);
    if (spvReflectEnumerateEntryPointPushConstantBlocks(&Guard.Module, Name.c_str(), &Count, std::data(Blocks)) != SPV_REFLECT_RESULT_SUCCESS)
    {
        return Result;
    }

    for (const SpvReflectBlockVariable* const Block : Blocks)
    {
        Result.PushConstantSize = std::max(Result.PushConstantSize, Block->offset + Block->size);
        for (std::uint32_t Index = 0U; Index < Block->member_count; ++Index)
        {
            const SpvReflectBlockVariable& Member = Block->members[Index];
            Result.PushMembers.push_back({.Name = Member.name != nullptr ? Member.name : "", .Offset = Member.offset, .Size = Member.size});
        }
    }

    std::ranges::sort(Result.PushMembers, {}, &PushMember::Offset);
    return Result;
}
