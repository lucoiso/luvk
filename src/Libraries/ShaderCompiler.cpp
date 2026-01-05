/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Libraries/ShaderCompiler.hpp"
#include <atomic>
#include <cstring>
#include <slang-com-ptr.h>
#include <slang.h>
#include <stdexcept>

using namespace luvk;

Slang::ComPtr<slang::IGlobalSession> GGlobalSession;static constinit std::atomic_uint GInitCount{0};void luvk::InitializeShaderCompiler()
{
    if (GInitCount.fetch_add(1) == 0)
    {
        if (slang::createGlobalSession(GGlobalSession.writeRef()) != SLANG_OK)
        {
            GInitCount.fetch_sub(1);
            throw std::runtime_error("Failed to initialize Slang Global Session");
        }
    }
}

void luvk::ShutdownShaderCompiler()
{
    if (GInitCount.fetch_sub(1) == 1)
    {
        GGlobalSession = nullptr;
    }
}

CompilationResult luvk::CompileShaderSafe(std::string_view Source)
{
    CompilationResult Output{};
    if (!GGlobalSession)
    {
        throw std::runtime_error("Shader compiler not initialized");
    }

    Slang::ComPtr<slang::ISession> Session;
    static const slang::TargetDesc Target{.format = SLANG_SPIRV,
                                          .profile = GGlobalSession->findProfile("spirv_1_6"),
                                          .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY};

    slang::CompilerOptionEntry Option{.name = slang::CompilerOptionName::Optimization,
                                      .value = {.kind = slang::CompilerOptionValueKind::Int, .intValue0 = SLANG_OPTIMIZATION_LEVEL_MAXIMAL}};

    const slang::SessionDesc Desc{.targets = &Target,
                                  .targetCount = 1,
                                  .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                                  .compilerOptionEntries = &Option,
                                  .compilerOptionEntryCount = 1};

    if (GGlobalSession->createSession(Desc, Session.writeRef()) != SLANG_OK)
    {
        throw std::runtime_error("Failed to create slang session");
    }

    Slang::ComPtr<ISlangBlob> Diagnostics;
    const Slang::ComPtr Module{Session->loadModuleFromSourceString("shader_source", nullptr, std::data(Source), Diagnostics.writeRef())};

    if (Diagnostics)
    {
        Output.Error = static_cast<const char*>(Diagnostics->getBufferPointer());
    }

    if (!Module)
    {
        return Output;
    }

    Slang::ComPtr<ISlangBlob> Spirv;
    if (Module->getTargetCode(0, Spirv.writeRef(), Diagnostics.writeRef()) != SLANG_OK)
    {
        if (Diagnostics) Output.Error = static_cast<const char*>(Diagnostics->getBufferPointer());
        return Output;
    }

    if (Spirv)
    {
        Output.Data.resize(Spirv->getBufferSize() / sizeof(std::uint32_t));
        std::memcpy(std::data(Output.Data), Spirv->getBufferPointer(), Spirv->getBufferSize());
        Output.Result = true;
    }

    return Output;
}

std::vector<std::uint32_t> luvk::CompileShader(std::string_view Source)
{
    return CompileShaderSafe(Source).Data;
}
