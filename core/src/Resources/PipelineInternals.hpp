// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include <utility>
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Vulkan.hpp"
#include "luvk/Resources/Pipeline.hpp"

namespace luvk
{
    /** Owns a temporary VkShaderModule for the duration of pipeline creation (exception safe). */
    class ShaderModule
    {
        /** Device that owns the shader module (used when destroying it). */
        VkDevice m_Device = VK_NULL_HANDLE;

        /** Vulkan handle of the compiled shader module. */
        VkShaderModule m_Module = VK_NULL_HANDLE;

    public:
        /** Constructor. */
        ShaderModule() = default;

        /** Constructor. Creates the module from SPIR-V words (throws when the code is not valid SPIR-V). */
        ShaderModule(const VkDevice Device, const std::span<const std::uint32_t> Code)
            : m_Device(Device)
        {
            constexpr std::uint32_t SpirvMagic = 0x07230203U;
            if (std::size(Code) < 5U || Code[0] != SpirvMagic)
            {
                throw std::invalid_argument("[luvk] Shader code is not valid SPIR-V.");
            }

            const VkShaderModuleCreateInfo Info{.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                .codeSize = std::size(Code) * sizeof(std::uint32_t),
                                                .pCode    = std::data(Code)};
            luvk::Check(vkCreateShaderModule(m_Device, &Info, nullptr, &m_Module), "vkCreateShaderModule");
        }

        /** Destructor. Destroys the owned shader module. */
        ~ShaderModule()
        {
            if (m_Module != VK_NULL_HANDLE)
            {
                vkDestroyShaderModule(m_Device, m_Module, nullptr);
            }
        }

        /** Non-copyable. */
        ShaderModule(const ShaderModule&)            = delete;

        /** Non-copyable. */
        ShaderModule& operator=(const ShaderModule&) = delete;

        /** Move-only. Transfers the module handle. */
        ShaderModule(ShaderModule&& Other) noexcept
            : m_Device(Other.m_Device),
              m_Module(std::exchange(Other.m_Module, VK_NULL_HANDLE)) {}

        /** Move-only. */
        ShaderModule& operator=(ShaderModule&&) = delete;

        /** Returns the Vulkan handle of the shader module. */
        [[nodiscard]] VkShaderModule Handle() const noexcept
        {
            return m_Module;
        }
    };

    /** Specialization data of one stage: map entries pointing into a tightly packed array of 32-bit values. */
    struct StageSpecialization
    {
        /** One entry per constant (offset = index * 4, size = 4). */
        std::vector<VkSpecializationMapEntry> Entries;

        /** Constant values, in the order of Entries. */
        std::vector<std::uint32_t> Values;
    };

    /** Validates the constants of Stage (unique ids) and packs them for VkSpecializationInfo. */
    inline StageSpecialization PackSpecialization(const luvk::ShaderStage& Stage)
    {
        StageSpecialization Result;
        Result.Entries.reserve(std::size(Stage.Constants));
        Result.Values.reserve(std::size(Stage.Constants));

        for (const luvk::SpecializationConstant& Constant : Stage.Constants)
        {
            for (const VkSpecializationMapEntry& Existing : Result.Entries)
            {
                if (Existing.constantID == Constant.Id)
                {
                    throw std::invalid_argument("[luvk] Duplicate specialization constant id " + std::to_string(Constant.Id) + ".");
                }
            }

            Result.Entries.push_back(VkSpecializationMapEntry{.constantID = Constant.Id,
                                                              .offset     = static_cast<std::uint32_t>(std::size(Result.Values) * sizeof(std::uint32_t)),
                                                              .size       = sizeof(std::uint32_t)});
            Result.Values.push_back(Constant.Value);
        }

        return Result;
    }

    /** Builds the VkSpecializationInfo of Data (pointing into it), or an empty one when there are no constants. */
    inline VkSpecializationInfo MakeSpecializationInfo(const StageSpecialization& Data) noexcept
    {
        return VkSpecializationInfo{.mapEntryCount = static_cast<std::uint32_t>(std::size(Data.Entries)),
                                    .pMapEntries   = std::data(Data.Entries),
                                    .dataSize      = std::size(Data.Values) * sizeof(std::uint32_t),
                                    .pData         = std::data(Data.Values)};
    }

    /** Accumulates the shader stages of one pipeline creation. */
    struct StageBuilder
    {
        /** Shader modules of the added stages (kept alive until the pipeline is created). */
        std::vector<ShaderModule> Modules;

        /** Entry point names matching Modules. */
        std::vector<std::string> EntryPoints;

        /** Specialization constants matching Modules. */
        std::vector<StageSpecialization> Specializations;

        /** Specialization infos built by Finalize (pointing into Specializations). */
        std::vector<VkSpecializationInfo> SpecializationInfos;

        /** Stage create infos built by Finalize (pointing into Modules, EntryPoints and SpecializationInfos). */
        std::vector<VkPipelineShaderStageCreateInfo> Stages;

        /** Creates and stores the shader module of one stage (ignored when the stage is not set). */
        void Add(const VkDevice Device, const luvk::ShaderStage& Stage)
        {
            if (!Stage.IsSet())
            {
                return;
            }

            Specializations.push_back(PackSpecialization(Stage));
            Modules.emplace_back(Device, Stage.Code);
            EntryPoints.emplace_back(Stage.EntryPoint);
        }

        /** Builds one VkPipelineShaderStageCreateInfo per module, pairing it with its stage flag, entry point and constants. */
        void Finalize(const std::span<const VkShaderStageFlagBits> Flags)
        {
            Stages.clear();
            SpecializationInfos.clear();
            SpecializationInfos.reserve(std::size(Modules)); // Stages point into this vector: it must not reallocate.

            for (std::size_t Index = 0U; Index < std::size(Modules); ++Index)
            {
                const bool HasConstants = !std::empty(Specializations[Index].Entries);
                if (HasConstants)
                {
                    SpecializationInfos.push_back(MakeSpecializationInfo(Specializations[Index]));
                }

                Stages.push_back(VkPipelineShaderStageCreateInfo{.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                                 .stage               = Flags[Index],
                                                                 .module              = Modules[Index].Handle(),
                                                                 .pName               = EntryPoints[Index].c_str(),
                                                                 .pSpecializationInfo = HasConstants ? &SpecializationInfos.back() : nullptr});
            }
        }
    };
}
