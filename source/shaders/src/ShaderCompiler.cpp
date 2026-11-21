// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "ShaderCompilerInternals.hpp"
#include "luvk/Shaders/ShaderCompiler.hpp"
#include <algorithm>
#include <charconv>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <slang-com-ptr.h>
#include <slang.h>
#include "LuvkModuleSource.hpp"

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

    /** One known stage name. */
    struct StageName
    {
        std::string_view Name;
        SlangStage       Stage;
    };

    constexpr StageName Stages[] = {{"vertex", SLANG_STAGE_VERTEX},
                                    {"fragment", SLANG_STAGE_FRAGMENT},
                                    {"pixel", SLANG_STAGE_FRAGMENT},
                                    {"compute", SLANG_STAGE_COMPUTE},
                                    {"mesh", SLANG_STAGE_MESH},
                                    {"task", SLANG_STAGE_AMPLIFICATION},
                                    {"amplification", SLANG_STAGE_AMPLIFICATION},
                                    {"geometry", SLANG_STAGE_GEOMETRY},
                                    {"hull", SLANG_STAGE_HULL},
                                    {"domain", SLANG_STAGE_DOMAIN},
                                    {"raygen", SLANG_STAGE_RAY_GENERATION},
                                    {"closesthit", SLANG_STAGE_CLOSEST_HIT},
                                    {"anyhit", SLANG_STAGE_ANY_HIT},
                                    {"miss", SLANG_STAGE_MISS},
                                    {"intersection", SLANG_STAGE_INTERSECTION},
                                    {"callable", SLANG_STAGE_CALLABLE}};

    /** Slang stage of a ShaderCompileOptions::Stage name (nullopt for unknown names). */
    std::optional<SlangStage> ToSlangStage(const std::string_view Name)
    {
        for (const StageName& It : Stages)
        {
            if (It.Name == Name)
            {
                return It.Stage;
            }
        }
        return std::nullopt;
    }

    /** Name of a Slang stage. */
    std::string_view StageToName(const SlangStage Stage)
    {
        for (const StageName& It : Stages)
        {
            if (It.Stage == Stage)
            {
                return It.Name;
            }
        }
        return "unknown";
    }

    /** UTF-8 path string for Slang (std::filesystem::path::string() uses the ANSI code page on Windows). */
    std::string ToUtf8(const std::filesystem::path& Path)
    {
        const std::u8string Text = Path.generic_u8string();
        return std::string(std::begin(Text), std::end(Text));
    }

    /** Path from a UTF-8 string. */
    std::filesystem::path FromUtf8(const std::string_view Text)
    {
        return std::filesystem::path(std::u8string(std::begin(Text), std::end(Text)));
    }

    /** 64-bit FNV-1a, chained through Seed. */
    std::uint64_t Hash(const std::string_view Data, std::uint64_t Seed = 14695981039346656037ULL) noexcept
    {
        for (const char Character : Data)
        {
            Seed = (Seed ^ static_cast<std::uint8_t>(Character)) * 1099511628211ULL;
        }
        return Seed;
    }

    /** Hash as 16 hexadecimal digits. */
    std::string HexOf(const std::uint64_t Value)
    {
        char Text[17] = {};
        std::to_chars(std::begin(Text), std::end(Text) - 1, Value, 16);
        std::string Result(Text);
        return std::string(16U - std::size(Result), '0') + Result;
    }

    /** Whole file as bytes, or nullopt. */
    std::optional<std::string> ReadFile(const std::filesystem::path& Path)
    {
        std::ifstream File(Path, std::ios::binary);
        if (!File)
        {
            return std::nullopt;
        }
        return std::string{std::istreambuf_iterator<char>(File), std::istreambuf_iterator<char>()};
    }

    /** Writes Data to Path through a temporary file renamed over it (a crash never leaves a torn file). */
    bool WriteFileAtomically(const std::filesystem::path& Path, const std::string_view Data)
    {
        std::filesystem::path Temporary = Path;
        Temporary += ".tmp";
        {
            std::ofstream File(Temporary, std::ios::binary | std::ios::trunc);
            if (!File || !File.write(std::data(Data), static_cast<std::streamsize>(std::size(Data))))
            {
                return false;
            }
        }
        std::error_code Error;
        std::filesystem::rename(Temporary, Path, Error);
        return !Error;
    }

    /** Escapes tabs, newlines and backslashes of a cache field. */
    std::string Escape(const std::string_view Text)
    {
        std::string Result;
        Result.reserve(std::size(Text));
        for (const char Character : Text)
        {
            switch (Character)
            {
            case '\\': Result += "\\\\"; break;
            case '\t': Result += "\\t"; break;
            case '\n': Result += "\\n"; break;
            default: Result += Character; break;
            }
        }
        return Result;
    }

    /** Inverse of Escape. */
    std::string Unescape(const std::string_view Text)
    {
        std::string Result;
        Result.reserve(std::size(Text));
        for (std::size_t Index = 0U; Index < std::size(Text); ++Index)
        {
            if (Text[Index] == '\\' && Index + 1U < std::size(Text))
            {
                const char Next = Text[++Index];
                Result += Next == 't' ? '\t' : Next == 'n' ? '\n' : Next;
                continue;
            }
            Result += Text[Index];
        }
        return Result;
    }

    /** Splits a cache line at tabs. */
    std::vector<std::string_view> SplitFields(const std::string_view Line)
    {
        std::vector<std::string_view> Fields;
        std::size_t                   Start = 0U;
        while (Start <= std::size(Line))
        {
            const std::size_t End = Line.find('\t', Start);
            Fields.push_back(Line.substr(Start, End == std::string_view::npos ? std::string_view::npos : End - Start));
            if (End == std::string_view::npos)
            {
                break;
            }
            Start = End + 1U;
        }
        return Fields;
    }

    /** Unsigned integer of a cache field (0 when malformed). */
    std::uint64_t ToNumber(const std::string_view Text, const int Base = 10)
    {
        std::uint64_t Value = 0U;
        std::from_chars(std::data(Text), std::data(Text) + std::size(Text), Value, Base);
        return Value;
    }

    /** Reads the user attributes of a reflected variable. */
    std::vector<luvk::ShaderAttribute> ReflectAttributes(slang::VariableReflection* const Variable)
    {
        std::vector<luvk::ShaderAttribute> Result;
        if (Variable == nullptr)
        {
            return Result;
        }

        for (unsigned int Index = 0U; Index < Variable->getUserAttributeCount(); ++Index)
        {
            slang::UserAttribute* const Attribute = Variable->getUserAttributeByIndex(Index);
            if (Attribute == nullptr || Attribute->getName() == nullptr)
            {
                continue;
            }

            luvk::ShaderAttribute Entry{.Name = Attribute->getName()};
            if (Entry.Name.ends_with("Attribute"))
            {
                Entry.Name.resize(std::size(Entry.Name) - 9U);
            }

            for (unsigned int Argument = 0U; Argument < Attribute->getArgumentCount(); ++Argument)
            {
                std::size_t       Length = 0U;
                float             Float  = 0.F;
                int               Int    = 0;
                if (const char* const Text = Attribute->getArgumentValueString(Argument, &Length); Text != nullptr)
                {
                    std::string Value(Text, Length);
                    if (std::size(Value) >= 2U && Value.front() == '"' && Value.back() == '"')
                    {
                        Value = Value.substr(1U, std::size(Value) - 2U);
                    }
                    Entry.Strings.push_back(std::move(Value));
                }
                else if (SLANG_SUCCEEDED(Attribute->getArgumentValueFloat(Argument, &Float)))
                {
                    Entry.Numbers.push_back(static_cast<double>(Float));
                }
                else if (SLANG_SUCCEEDED(Attribute->getArgumentValueInt(Argument, &Int)))
                {
                    Entry.Numbers.push_back(static_cast<double>(Int));
                }
            }
            Result.push_back(std::move(Entry));
        }
        return Result;
    }

    /** Reflects a struct type layout as a block; pointer fields leading to structs are appended as further blocks. */
    void ReflectBlock(luvk::ShaderProgramReflection& Program, const std::string_view Name, slang::TypeLayoutReflection* const Struct, const std::uint32_t Depth)
    {
        if (Struct == nullptr || Struct->getKind() != slang::TypeReflection::Kind::Struct || Depth > 4U)
        {
            return;
        }

        luvk::ShaderBlock Block{.Name = std::string(Name),
                                .Type = Struct->getName() != nullptr ? Struct->getName() : "",
                                .Size = static_cast<std::uint32_t>(Struct->getSize())};

        std::vector<std::pair<std::string, slang::TypeLayoutReflection*>> Pointees;
        for (unsigned int Index = 0U; Index < Struct->getFieldCount(); ++Index)
        {
            slang::VariableLayoutReflection* const Field = Struct->getFieldByIndex(Index);
            if (Field == nullptr)
            {
                continue;
            }

            slang::TypeLayoutReflection* const Type = Field->getTypeLayout();
            luvk::ShaderField                  Entry{.Name       = Field->getName() != nullptr ? Field->getName() : "",
                                                     .Type       = Type != nullptr && Type->getName() != nullptr ? Type->getName() : "",
                                                     .Offset     = static_cast<std::uint32_t>(Field->getOffset()),
                                                     .Size       = Type != nullptr ? static_cast<std::uint32_t>(Type->getSize()) : 0U,
                                                     .Attributes = ReflectAttributes(Field->getVariable())};

            if (Type != nullptr && Type->getKind() == slang::TypeReflection::Kind::Pointer)
            {
                if (slang::TypeLayoutReflection* const Target = Type->getElementTypeLayout(); Target != nullptr)
                {
                    Pointees.emplace_back(Entry.Name, Target);
                }
            }
            Block.Fields.push_back(std::move(Entry));
        }

        Program.Blocks.push_back(std::move(Block));
        for (const auto& [FieldName, Target] : Pointees)
        {
            ReflectBlock(Program, FieldName, Target, Depth + 1U);
        }
    }

    /** Entry points and blocks of a linked program. */
    luvk::ShaderProgramReflection ReflectProgram(slang::IComponentType* const Linked)
    {
        luvk::ShaderProgramReflection Program;
        slang::ProgramLayout* const   Layout = Linked->getLayout();
        if (Layout == nullptr)
        {
            return Program;
        }

        for (SlangUInt Index = 0U; Index < Layout->getEntryPointCount(); ++Index)
        {
            slang::EntryPointReflection* const Entry = Layout->getEntryPointByIndex(Index);
            if (Entry == nullptr)
            {
                continue;
            }

            luvk::ShaderEntry Reflected{.Name = Entry->getName() != nullptr ? Entry->getName() : "", .Stage = std::string(StageToName(Entry->getStage()))};
            if (Entry->getStage() == SLANG_STAGE_COMPUTE || Entry->getStage() == SLANG_STAGE_MESH || Entry->getStage() == SLANG_STAGE_AMPLIFICATION)
            {
                SlangUInt Sizes[3] = {1U, 1U, 1U};
                Entry->getComputeThreadGroupSize(3U, Sizes);
                Reflected.ThreadGroupSize = {static_cast<std::uint32_t>(Sizes[0]), static_cast<std::uint32_t>(Sizes[1]), static_cast<std::uint32_t>(Sizes[2])};
            }
            Program.EntryPoints.push_back(std::move(Reflected));
        }

        for (unsigned int Index = 0U; Index < Layout->getParameterCount(); ++Index)
        {
            slang::VariableLayoutReflection* const Parameter = Layout->getParameterByIndex(Index);
            if (Parameter == nullptr || Parameter->getCategory() != slang::ParameterCategory::PushConstantBuffer)
            {
                continue;
            }

            slang::TypeLayoutReflection* const Buffer = Parameter->getTypeLayout();
            ReflectBlock(Program, "push", Buffer != nullptr ? Buffer->getElementTypeLayout() : nullptr, 0U);
        }
        return Program;
    }

    /** Serializes a result for the cache. */
    std::string SerializeCache(const luvk::ShaderCompileResult& Result, const std::uint64_t SpirvHash, const std::vector<std::uint64_t>& DependencyHashes)
    {
        std::ostringstream Text;
        Text << "luvk-shader-cache 1\n";
        for (std::size_t Index = 0U; Index < std::size(Result.Dependencies); ++Index)
        {
            Text << "dep\t" << HexOf(DependencyHashes[Index]) << '\t' << Escape(ToUtf8(Result.Dependencies[Index])) << '\n';
        }
        for (const luvk::ShaderEntry& Entry : Result.Reflection.EntryPoints)
        {
            Text << "entry\t" << Escape(Entry.Name) << '\t' << Entry.Stage << '\t' << Entry.ThreadGroupSize[0] << '\t' << Entry.ThreadGroupSize[1] << '\t'
                 << Entry.ThreadGroupSize[2] << '\n';
        }
        for (const luvk::ShaderBlock& Block : Result.Reflection.Blocks)
        {
            Text << "block\t" << Escape(Block.Name) << '\t' << Escape(Block.Type) << '\t' << Block.Size << '\n';
            for (const luvk::ShaderField& Field : Block.Fields)
            {
                Text << "field\t" << Escape(Field.Name) << '\t' << Escape(Field.Type) << '\t' << Field.Offset << '\t' << Field.Size << '\n';
                for (const luvk::ShaderAttribute& Attribute : Field.Attributes)
                {
                    Text << "attr\t" << Escape(Attribute.Name) << '\t' << std::size(Attribute.Numbers);
                    for (const double Number : Attribute.Numbers)
                    {
                        Text << '\t' << Number;
                    }
                    Text << '\t' << std::size(Attribute.Strings);
                    for (const std::string& String : Attribute.Strings)
                    {
                        Text << '\t' << Escape(String);
                    }
                    Text << '\n';
                }
            }
        }
        Text << "spirv\t" << HexOf(SpirvHash) << '\n';
        return Text.str();
    }

    /** Loads a cache entry when every dependency still has the recorded content; nullopt otherwise. */
    std::optional<luvk::ShaderCompileResult> LoadCache(const std::filesystem::path& Directory, const std::string& Key)
    {
        const std::optional<std::string> Index = ReadFile(Directory / (Key + ".cache"));
        if (!Index.has_value() || !Index->starts_with("luvk-shader-cache 1\n"))
        {
            return std::nullopt;
        }

        luvk::ShaderCompileResult Result{.FromCache = true};
        std::string               SpirvName;
        std::istringstream        Lines(*Index);
        std::string               Line;
        std::getline(Lines, Line);
        while (std::getline(Lines, Line))
        {
            const std::vector<std::string_view> Fields = SplitFields(Line);
            const std::string_view              Kind   = Fields.front();

            if (Kind == "dep" && std::size(Fields) == 3U)
            {
                const std::filesystem::path       Path    = FromUtf8(Unescape(Fields[2]));
                const std::optional<std::string>  Content = ReadFile(Path);
                if (!Content.has_value() || ToNumber(Fields[1], 16) != Hash(*Content))
                {
                    return std::nullopt;
                }
                Result.Dependencies.push_back(Path);
            }
            else if (Kind == "entry" && std::size(Fields) == 6U)
            {
                Result.Reflection.EntryPoints.push_back({.Name            = Unescape(Fields[1]),
                                                         .Stage           = std::string(Fields[2]),
                                                         .ThreadGroupSize = {static_cast<std::uint32_t>(ToNumber(Fields[3])), static_cast<std::uint32_t>(ToNumber(Fields[4])),
                                                                             static_cast<std::uint32_t>(ToNumber(Fields[5]))}});
            }
            else if (Kind == "block" && std::size(Fields) == 4U)
            {
                Result.Reflection.Blocks.push_back({.Name = Unescape(Fields[1]), .Type = Unescape(Fields[2]), .Size = static_cast<std::uint32_t>(ToNumber(Fields[3]))});
            }
            else if (Kind == "field" && std::size(Fields) == 5U && !std::empty(Result.Reflection.Blocks))
            {
                Result.Reflection.Blocks.back().Fields.push_back({.Name   = Unescape(Fields[1]),
                                                                  .Type   = Unescape(Fields[2]),
                                                                  .Offset = static_cast<std::uint32_t>(ToNumber(Fields[3])),
                                                                  .Size   = static_cast<std::uint32_t>(ToNumber(Fields[4]))});
            }
            else if (Kind == "attr" && std::size(Fields) >= 3U && !std::empty(Result.Reflection.Blocks) && !std::empty(Result.Reflection.Blocks.back().Fields))
            {
                luvk::ShaderAttribute Attribute{.Name = Unescape(Fields[1])};
                std::size_t           Cursor  = 2U;
                const std::size_t     Numbers = static_cast<std::size_t>(ToNumber(Fields[Cursor++]));
                for (std::size_t Number = 0U; Number < Numbers && Cursor < std::size(Fields); ++Number, ++Cursor)
                {
                    Attribute.Numbers.push_back(std::stod(std::string(Fields[Cursor])));
                }
                const std::size_t Strings = Cursor < std::size(Fields) ? static_cast<std::size_t>(ToNumber(Fields[Cursor++])) : 0U;
                for (std::size_t String = 0U; String < Strings && Cursor < std::size(Fields); ++String, ++Cursor)
                {
                    Attribute.Strings.push_back(Unescape(Fields[Cursor]));
                }
                Result.Reflection.Blocks.back().Fields.back().Attributes.push_back(std::move(Attribute));
            }
            else if (Kind == "spirv" && std::size(Fields) == 2U)
            {
                SpirvName = std::string(Fields[1]);
            }
        }

        const std::optional<std::string> Spirv = std::empty(SpirvName) ? std::nullopt : ReadFile(Directory / (SpirvName + ".spv"));
        if (!Spirv.has_value() || std::empty(*Spirv) || std::size(*Spirv) % sizeof(std::uint32_t) != 0U)
        {
            return std::nullopt;
        }

        Result.Spirv.resize(std::size(*Spirv) / sizeof(std::uint32_t));
        std::memcpy(std::data(Result.Spirv), std::data(*Spirv), std::size(*Spirv));
        return Result;
    }

    /** Stores a result in the cache (failures only cost the next compilation). */
    void StoreCache(const std::filesystem::path& Directory, const std::string& Key, const luvk::ShaderCompileResult& Result)
    {
        std::error_code Error;
        std::filesystem::create_directories(Directory, Error);

        std::vector<std::uint64_t> DependencyHashes;
        DependencyHashes.reserve(std::size(Result.Dependencies));
        for (const std::filesystem::path& Dependency : Result.Dependencies)
        {
            const std::optional<std::string> Content = ReadFile(Dependency);
            if (!Content.has_value())
            {
                return;
            }
            DependencyHashes.push_back(Hash(*Content));
        }

        const std::string_view Bytes(reinterpret_cast<const char*>(std::data(Result.Spirv)), std::size(Result.Spirv) * sizeof(std::uint32_t));
        const std::uint64_t    SpirvHash = Hash(Bytes);
        if (WriteFileAtomically(Directory / (HexOf(SpirvHash) + ".spv"), Bytes))
        {
            WriteFileAtomically(Directory / (Key + ".cache"), SerializeCache(Result, SpirvHash, DependencyHashes));
        }
    }

    /** One compilation: source text, the path it is loaded from (empty for in-memory sources) and the options. */
    struct CompileRequest
    {
        /** Slang source code. */
        std::string_view Source;

        /** File the source comes from (empty for in-memory sources). */
        std::filesystem::path Path;

        /** Compile options. */
        const luvk::ShaderCompileOptions& Options;
    };

    /** Cache key of a request: compiler build, options, and the source itself for in-memory sources. */
    std::string CacheKey(const CompileRequest& Request, const std::string_view CompilerTag)
    {
        const luvk::ShaderCompileOptions& Options = Request.Options;
        std::uint64_t                     Key     = Hash(CompilerTag);
        Key = Hash(Request.Path.empty() ? Request.Source : std::string_view(ToUtf8(Request.Path)), Key);
        Key = Hash(Options.Profile, Key);
        Key = Hash(Options.ModuleName, Key);
        Key = Hash(Options.EntryPoint, Key);
        Key = Hash(Options.Stage, Key);
        Key = Hash(Options.Optimize ? "O" : "-", Key);
        for (const std::string& Path : Options.SearchPaths)
        {
            Key = Hash(Path, Hash("|", Key));
        }
        for (const auto& [Name, Value] : Options.Defines)
        {
            Key = Hash(Value, Hash(Name, Hash("#", Key)));
        }
        Key = Hash(luvk::LuvkShaderModule(), Key);
        return HexOf(Key);
    }

    /** Shared implementation of CompileShader and CompileShaderFile (the caller holds no lock). */
    std::expected<luvk::ShaderCompileResult, std::string> Compile(const CompileRequest& Request)
    {
        SlangState& Slang = State();
        if (SLANG_FAILED(Slang.Result) || !Slang.GlobalSession)
        {
            return std::unexpected("[luvk] Failed to create the Slang global session.");
        }

        const luvk::ShaderCompileOptions& Options = Request.Options;

        std::optional<SlangStage> Stage;
        if (!std::empty(Options.Stage))
        {
            Stage = ToSlangStage(Options.Stage);
            if (!Stage.has_value())
            {
                return std::unexpected("[luvk] Unknown shader stage '" + std::string(Options.Stage) + "'.");
            }
        }

        const char* const Tag      = Slang.GlobalSession->getBuildTagString();
        const std::string Key      = Options.CacheDirectory.empty() ? std::string{} : CacheKey(Request, Tag != nullptr ? Tag : "slang");
        if (!std::empty(Key))
        {
            if (std::optional<luvk::ShaderCompileResult> Cached = LoadCache(Options.CacheDirectory, Key))
            {
                return std::move(*Cached);
            }
        }

        std::scoped_lock Lock(Slang.Mutex);

        const std::string Profile(Options.Profile);
        const std::string EntryPoint(Options.EntryPoint);
        const std::string Code(Request.Source);
        const std::string ModuleCode(luvk::LuvkShaderModule());

        std::string ModuleName(Options.ModuleName);
        std::string ModulePath = ModuleName + ".slang";
        if (!Request.Path.empty())
        {
            ModulePath = ToUtf8(Request.Path);
            if (Options.ModuleName == luvk::ShaderCompileOptions{}.ModuleName)
            {
                ModuleName = ToUtf8(Request.Path.stem());
            }
        }

        // The file's own directory resolves its imports and includes first.
        std::vector<std::string> SearchPathStorage;
        if (!Request.Path.empty() && Request.Path.has_parent_path())
        {
            SearchPathStorage.push_back(ToUtf8(Request.Path.parent_path()));
        }
        SearchPathStorage.insert(std::end(SearchPathStorage), std::begin(Options.SearchPaths), std::end(Options.SearchPaths));

        std::vector<const char*> SearchPaths;
        SearchPaths.reserve(std::size(SearchPathStorage));
        for (const std::string& It : SearchPathStorage)
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

        Slang::ComPtr<slang::IModule> LuvkModule(Session->loadModuleFromSourceString("luvk", "luvk.slang", ModuleCode.c_str(), Diagnostics.writeRef()));
        const std::string             ModuleDiagnostics = LuvkModule ? std::string{} : "[luvk] luvk module: " + ToString(Diagnostics) + '\n';

        Slang::ComPtr<slang::IModule> Module(Session->loadModuleFromSourceString(ModuleName.c_str(), ModulePath.c_str(), Code.c_str(), Diagnostics.writeRef()));
        if (!Module)
        {
            return std::unexpected(ModuleDiagnostics + ToString(Diagnostics));
        }

        std::vector<Slang::ComPtr<slang::IEntryPoint>> Entries;
        if (std::empty(EntryPoint))
        {
            for (SlangInt32 Index = 0; Index < Module->getDefinedEntryPointCount(); ++Index)
            {
                Slang::ComPtr<slang::IEntryPoint> Entry;
                if (SLANG_SUCCEEDED(Module->getDefinedEntryPoint(Index, Entry.writeRef())))
                {
                    Entries.push_back(std::move(Entry));
                }
            }
        }
        else
        {
            Slang::ComPtr<slang::IEntryPoint> Entry;
            if (SLANG_FAILED(Module->findEntryPointByName(EntryPoint.c_str(), Entry.writeRef())))
            {
                // Functions without a [shader("...")] attribute need their stage spelled out.
                if (!Stage.has_value() || SLANG_FAILED(Module->findAndCheckEntryPoint(EntryPoint.c_str(), *Stage, Entry.writeRef(), Diagnostics.writeRef())))
                {
                    return std::unexpected("[luvk] Entry point not found: " + EntryPoint +
                                           (Stage.has_value() ? "\n" + ToString(Diagnostics) : std::string(" (set ShaderCompileOptions::Stage when it has no [shader] attribute)")));
                }
            }
            Entries.push_back(std::move(Entry));
        }

        if (std::empty(Entries))
        {
            return std::unexpected("[luvk] The module declares no entry point.");
        }

        std::vector<slang::IComponentType*> Components = {Module.get()};
        for (const Slang::ComPtr<slang::IEntryPoint>& Entry : Entries)
        {
            Components.push_back(Entry.get());
        }

        Slang::ComPtr<slang::IComponentType> Composite;
        if (SLANG_FAILED(Session->createCompositeComponentType(std::data(Components), static_cast<SlangInt>(std::size(Components)), Composite.writeRef(),
                                                               Diagnostics.writeRef())))
        {
            return std::unexpected(ToString(Diagnostics));
        }

        Slang::ComPtr<slang::IComponentType> Linked;
        if (SLANG_FAILED(Composite->link(Linked.writeRef(), Diagnostics.writeRef())))
        {
            return std::unexpected(ToString(Diagnostics));
        }

        Slang::ComPtr<slang::IBlob> Spirv;
        const SlangResult           Generated = std::size(Entries) == 1U ? Linked->getEntryPointCode(0, 0, Spirv.writeRef(), Diagnostics.writeRef())
                                                                         : Linked->getTargetCode(0, Spirv.writeRef(), Diagnostics.writeRef());
        if (SLANG_FAILED(Generated) || !Spirv || Spirv->getBufferSize() == 0U || Spirv->getBufferSize() % sizeof(std::uint32_t) != 0U)
        {
            return std::unexpected("[luvk] Slang produced no valid SPIR-V. " + ToString(Diagnostics));
        }

        luvk::ShaderCompileResult Result;
        Result.Spirv.resize(Spirv->getBufferSize() / sizeof(std::uint32_t));
        std::memcpy(std::data(Result.Spirv), Spirv->getBufferPointer(), Spirv->getBufferSize());
        Result.Reflection = ReflectProgram(Linked.get());

        if (!Request.Path.empty())
        {
            // Every module the session loaded (the file, its imports) reports the files it read; in-memory modules such
            // as the luvk module have no file on disk and are skipped.
            const auto Add = [&](const std::filesystem::path& Candidate)
            {
                std::error_code Error;
                if (!std::filesystem::is_regular_file(Candidate, Error))
                {
                    return;
                }

                std::filesystem::path Normalized = std::filesystem::weakly_canonical(Candidate, Error);
                if (Error)
                {
                    Normalized = std::filesystem::absolute(Candidate, Error);
                }

                if (std::ranges::find(Result.Dependencies, Normalized) == std::end(Result.Dependencies))
                {
                    Result.Dependencies.push_back(std::move(Normalized));
                }
            };

            Add(Request.Path);
            for (SlangInt Index = 0; Index < Session->getLoadedModuleCount(); ++Index)
            {
                slang::IModule* const Loaded = Session->getLoadedModule(Index);
                if (Loaded == nullptr)
                {
                    continue;
                }

                for (SlangInt32 File = 0; File < Loaded->getDependencyFileCount(); ++File)
                {
                    if (const char* const Dependency = Loaded->getDependencyFilePath(File); Dependency != nullptr && *Dependency != '\0')
                    {
                        Add(FromUtf8(Dependency));
                    }
                }
            }
        }

        if (!std::empty(Key))
        {
            StoreCache(Options.CacheDirectory, Key, Result);
        }
        return Result;
    }
}

std::string_view luvk::LuvkShaderModule() noexcept
{
    return {std::data(LuvkModuleSource), std::size(LuvkModuleSource)};
}

const luvk::ShaderAttribute* luvk::ShaderField::Find(const std::string_view AttributeName) const noexcept
{
    const auto It = std::ranges::find(Attributes, AttributeName, &ShaderAttribute::Name);
    return It != std::end(Attributes) ? &*It : nullptr;
}

const luvk::ShaderBlock* luvk::ShaderProgramReflection::Find(const std::string_view BlockName) const noexcept
{
    const auto It = std::ranges::find(Blocks, BlockName, &ShaderBlock::Name);
    return It != std::end(Blocks) ? &*It : nullptr;
}

std::expected<luvk::ShaderCompileResult, std::string> luvk::CompileShader(const std::string_view Source, const ShaderCompileOptions& Options)
{
    return Compile(CompileRequest{.Source = Source, .Options = Options});
}

std::expected<luvk::ShaderCompileResult, std::string> luvk::CompileShaderFile(const std::filesystem::path& Path, const ShaderCompileOptions& Options)
{
    std::error_code             Error;
    const std::filesystem::path Absolute = std::filesystem::absolute(Path, Error);
    if (Error || !std::filesystem::is_regular_file(Absolute, Error))
    {
        return std::unexpected("[luvk] Shader file not found: " + ToUtf8(Path));
    }

    const std::optional<std::string> Source = ReadFile(Absolute);
    if (!Source.has_value())
    {
        return std::unexpected("[luvk] Cannot open shader file: " + ToUtf8(Absolute));
    }

    return Compile(CompileRequest{.Source = *Source, .Path = Absolute, .Options = Options});
}

std::vector<std::uint32_t> luvk::CompileShaderOrThrow(const std::string_view Source, const ShaderCompileOptions& Options)
{
    auto Result = CompileShader(Source, Options);
    if (!Result)
    {
        throw std::runtime_error("[luvk] Shader compilation failed (" + std::string(Options.ModuleName) + ":" + std::string(Options.EntryPoint) + "):\n" + Result.error());
    }
    return std::move(Result->Spirv);
}

luvk::FileWatcher::FileWatcher(const std::chrono::steady_clock::duration Interval)
    : m_Interval(Interval),
      m_LastScan(std::chrono::steady_clock::now())
{
}

void luvk::FileWatcher::Watch(const std::filesystem::path& Path)
{
    if (std::ranges::find(m_Files, Path, &Entry::Path) != std::end(m_Files))
    {
        return;
    }

    std::error_code Error;
    m_Files.push_back({.Path = Path, .WriteTime = std::filesystem::last_write_time(Path, Error)});
}

void luvk::FileWatcher::Clear() noexcept
{
    m_Files.clear();
    m_Changed.clear();
}

const std::vector<std::filesystem::path>& luvk::FileWatcher::Poll()
{
    m_Changed.clear();
    const auto Now = std::chrono::steady_clock::now();
    if (Now - m_LastScan < m_Interval)
    {
        return m_Changed;
    }
    m_LastScan = Now;

    for (Entry& File : m_Files)
    {
        std::error_code                       Error;
        const std::filesystem::file_time_type Time = std::filesystem::last_write_time(File.Path, Error);
        if (!Error && Time != File.WriteTime)
        {
            File.WriteTime = Time;
            m_Changed.push_back(File.Path);
        }
    }
    return m_Changed;
}
