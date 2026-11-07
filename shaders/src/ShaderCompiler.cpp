// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "ShaderCompilerInternals.hpp"
#include "luvk/Shaders/ShaderCompiler.hpp"
#include <cstring>
#include <mutex>
#include <stdexcept>
#include <slang-com-ptr.h>
#include <slang.h>

using luvk::SlangState;

namespace
{
    std::string ToString(ISlangBlob* const Blob)
    {
        if (Blob == nullptr || Blob->getBufferSize() == 0U)
        {
            return {};
        }
        return std::string(static_cast<const char*>(Blob->getBufferPointer()), Blob->getBufferSize());
    }

    SlangState& State()
    {
        static SlangState Instance;
        return Instance;
    }
}

std::expected<std::vector<std::uint32_t>, std::string> luvk::CompileShader(const std::string_view Source, const ShaderCompileOptions& Options)
{
    SlangState& Slang = State();
    if (SLANG_FAILED(Slang.Result) || !Slang.GlobalSession)
    {
        return std::unexpected("[luvk] Failed to create the Slang global session.");
    }

    std::scoped_lock Lock(Slang.Mutex);

    const std::string Profile(Options.Profile);
    const std::string ModuleName(Options.ModuleName);
    const std::string EntryPoint(Options.EntryPoint);
    const std::string Code(Source);
    const std::string BindlessCode(BindlessShaderModule);

    std::vector<const char*> SearchPaths;
    SearchPaths.reserve(std::size(Options.SearchPaths));
    for (const std::string& It : Options.SearchPaths)
    {
        SearchPaths.push_back(It.c_str());
    }

    std::vector<slang::PreprocessorMacroDesc> Macros;
    Macros.reserve(std::size(Options.Defines));
    for (const auto& [Name, Value] : Options.Defines)
    {
        Macros.push_back({Name.c_str(), Value.c_str()});
    }

    const slang::TargetDesc Target{.format  = SLANG_SPIRV,
                                   .profile = Slang.GlobalSession->findProfile(Profile.c_str()),
                                   .flags   = SLANG_TARGET_FLAG_GENERATE_SPIRV_DIRECTLY};

    const slang::CompilerOptionEntry Option{slang::CompilerOptionName::Optimization,
                                            {slang::CompilerOptionValueKind::Int,
                                             static_cast<std::int32_t>(Options.Optimize ? SLANG_OPTIMIZATION_LEVEL_HIGH : SLANG_OPTIMIZATION_LEVEL_NONE)}};

    const slang::SessionDesc SessionInfo{.targets                  = &Target,
                                         .targetCount              = 1,
                                         .defaultMatrixLayoutMode  = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
                                         .searchPaths              = std::data(SearchPaths),
                                         .searchPathCount          = static_cast<SlangInt>(std::size(SearchPaths)),
                                         .preprocessorMacros       = std::data(Macros),
                                         .preprocessorMacroCount   = static_cast<SlangInt>(std::size(Macros)),
                                         .compilerOptionEntries    = const_cast<slang::CompilerOptionEntry*>(&Option),
                                         .compilerOptionEntryCount = 1U};

    Slang::ComPtr<slang::ISession> Session;
    if (SLANG_FAILED(Slang.GlobalSession->createSession(SessionInfo, Session.writeRef())))
    {
        return std::unexpected("[luvk] Failed to create a Slang session.");
    }

    Slang::ComPtr<slang::IBlob> Diagnostics;

    Slang::ComPtr<slang::IModule> Bindless(Session->loadModuleFromSourceString("luvk", "luvk.slang", BindlessCode.c_str(), Diagnostics.writeRef()));
    const std::string             BindlessDiagnostics = Bindless ? std::string{} : "[luvk] bindless module: " + ToString(Diagnostics) + '\n';

    Slang::ComPtr<slang::IModule> Module(Session->loadModuleFromSourceString(ModuleName.c_str(), (ModuleName + ".slang").c_str(), Code.c_str(), Diagnostics.writeRef()));
    if (!Module)
    {
        return std::unexpected(BindlessDiagnostics + ToString(Diagnostics));
    }

    Slang::ComPtr<slang::IBlob> Spirv;

    if (std::empty(EntryPoint))
    {
        if (SLANG_FAILED(Module->getTargetCode(0, Spirv.writeRef(), Diagnostics.writeRef())))
        {
            return std::unexpected(ToString(Diagnostics));
        }
    }
    else
    {
        Slang::ComPtr<slang::IEntryPoint> Entry;
        if (SLANG_FAILED(Module->findEntryPointByName(EntryPoint.c_str(), Entry.writeRef())))
        {
            return std::unexpected("[luvk] Entry point not found: " + EntryPoint);
        }

        slang::IComponentType* const           Components[] = {Module.get(), Entry.get()};
        Slang::ComPtr<slang::IComponentType> Composite;
        if (SLANG_FAILED(Session->createCompositeComponentType(Components, 2, Composite.writeRef(), Diagnostics.writeRef())))
        {
            return std::unexpected(ToString(Diagnostics));
        }

        Slang::ComPtr<slang::IComponentType> Linked;
        if (SLANG_FAILED(Composite->link(Linked.writeRef(), Diagnostics.writeRef())))
        {
            return std::unexpected(ToString(Diagnostics));
        }

        if (SLANG_FAILED(Linked->getEntryPointCode(0, 0, Spirv.writeRef(), Diagnostics.writeRef())))
        {
            return std::unexpected(ToString(Diagnostics));
        }
    }

    if (!Spirv || Spirv->getBufferSize() == 0U || Spirv->getBufferSize() % sizeof(std::uint32_t) != 0U)
    {
        return std::unexpected("[luvk] Slang produced no valid SPIR-V. " + ToString(Diagnostics));
    }

    std::vector<std::uint32_t> Words(Spirv->getBufferSize() / sizeof(std::uint32_t));
    std::memcpy(std::data(Words), Spirv->getBufferPointer(), Spirv->getBufferSize());
    return Words;
}

std::vector<std::uint32_t> luvk::CompileShaderOrThrow(const std::string_view Source, const ShaderCompileOptions& Options)
{
    auto Result = CompileShader(Source, Options);
    if (!Result)
    {
        throw std::runtime_error("[luvk] Shader compilation failed (" + std::string(Options.ModuleName) + ":" + std::string(Options.EntryPoint) + "):\n" + Result.error());
    }
    return std::move(*Result);
}

