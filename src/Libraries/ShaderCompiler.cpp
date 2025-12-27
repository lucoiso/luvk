// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#ifdef LUVK_SLANG_INCLUDED

#include "luvk/Libraries/ShaderCompiler.hpp"
#include <atomic>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <slang-com-ptr.h>
#include <slang.h>
#include <stdexcept>

Slang::ComPtr<slang::IGlobalSession> GSlangGlobalSession;
static constinit std::atomic_uint    GSlangInitCount{0};

void luvk::InitializeShaderCompiler()
{
    if (GSlangInitCount.fetch_add(1) == 0)
    {
        if (slang::createGlobalSession(GSlangGlobalSession.writeRef()) != SLANG_OK)
        {
            GSlangInitCount.fetch_sub(1);
            throw std::runtime_error("Failed to initialize Slang Global Session.");
        }
    }
}

void luvk::ShutdownShaderCompiler()
{
    if (GSlangInitCount.fetch_sub(1) == 1)
    {
        GSlangGlobalSession = nullptr;
    }
}

luvk::CompilationResult luvk::CompileShaderSafe(const std::string_view Source, const std::string_view Profile)
{
    CompilationResult Output{};
    if (!GSlangGlobalSession)
    {
        throw std::runtime_error("Shader compiler not initialized. Call InitializeShaderCompiler() first.");
    }

    Slang::ComPtr<slang::ISession> SlangSession;

    const slang::TargetDesc Target{.format = SLANG_SPIRV,
                                   .profile = GSlangGlobalSession->findProfile(std::data(Profile)),
                                   .flags = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY};

    slang::CompilerOptionEntry Option{slang::CompilerOptionName::Optimization, {slang::CompilerOptionValueKind::Int, SLANG_OPTIMIZATION_LEVEL_MAXIMAL}};

    const slang::SessionDesc Desc{.targets = &Target,
                                  .targetCount = 1U,
                                  .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                                  .compilerOptionEntries = &Option,
                                  .compilerOptionEntryCount = 1U};

    if (GSlangGlobalSession->createSession(Desc, SlangSession.writeRef()) != SLANG_OK)
    {
        throw std::runtime_error("Failed to create slang session.");
    }

    Slang::ComPtr<ISlangBlob> LoadDiagnostics;
    const Slang::ComPtr       SlangModule{SlangSession->loadModuleFromSourceString("shader_source", nullptr, std::data(Source), LoadDiagnostics.writeRef())};

    if (LoadDiagnostics)
    {
        if (const auto DiagText = static_cast<const char*>(LoadDiagnostics->getBufferPointer());
            std::strlen(DiagText) > 0)
        {
            Output.Error = DiagText;
        }
    }

    if (!SlangModule)
    {
        return Output;
    }

    Slang::ComPtr<ISlangBlob> Spirv;
    Slang::ComPtr<ISlangBlob> Diagnostics;

    if (SlangModule->getTargetCode(0, Spirv.writeRef(), Diagnostics.writeRef()) != SLANG_OK)
    {
        if (Diagnostics)
        {
            if (const auto DiagText = static_cast<const char*>(Diagnostics->getBufferPointer());
                std::strlen(DiagText) > 0)
            {
                Output.Error = DiagText;
            }
        }

        return Output;
    }

    if (Spirv)
    {
        Output.Data.resize(Spirv->getBufferSize() / sizeof(std::uint32_t));
        std::memcpy(Output.Data.data(), Spirv->getBufferPointer(), Spirv->getBufferSize());

        Output.Result = true;

        return Output;
    }

    return Output;
}

std::vector<std::uint32_t> luvk::CompileShader(const std::string_view Source, const std::string_view Profile)
{
    return CompileShaderSafe(Source, Profile).Data;
}

#endif // LUVK_SLANG_INCLUDED
