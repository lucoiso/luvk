// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Libraries/ShaderCompiler.hpp"
#include <atomic>
#include <iterator>
#include <stdexcept>
#include <glslang/Public/ResourceLimits.h>
#include <glslang/SPIRV/GlslangToSpv.h>

static constinit std::atomic_uint g_GlslangInitCount{0};

void luvk::InitializeGlslang()
{
    if (g_GlslangInitCount.fetch_add(1) == 0)
    {
        glslang::InitializeProcess();
    }
}

void luvk::FinalizeGlslang()
{
    if (g_GlslangInitCount.fetch_sub(1) == 1)
    {
        glslang::FinalizeProcess();
    }
}

luvk::Vector<std::uint32_t> luvk::CompileGLSLToSPIRV(const std::string_view& Source, const EShLanguage Stage, const std::string_view& EntryPoint)
{
    const char* ShaderStr = std::data(Source);

    glslang::TShader Shader(Stage);
    Shader.setStrings(&ShaderStr, 1);
    Shader.setEntryPoint(std::data(EntryPoint));
    Shader.setSourceEntryPoint(std::data(EntryPoint));
    Shader.setEnvInput(glslang::EShSourceGlsl, Stage, glslang::EShClientVulkan, 100);
    Shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_4);
    Shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_6);

    const TBuiltInResource Resources = *GetDefaultResources();
    constexpr auto         Messages  = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

    if (!Shader.parse(&Resources, 100, false, Messages))
    {
        throw std::runtime_error(Shader.getInfoLog());
    }

    glslang::TProgram Program;
    Program.addShader(&Shader);
    if (!Program.link(Messages))
    {
        throw std::runtime_error(Program.getInfoLog());
    }

    Vector<std::uint32_t> Spirv{};
    glslang::SpvOptions   Options{};
    glslang::GlslangToSpv(*Program.getIntermediate(Stage), Spirv, &Options);
    return Spirv;
}
