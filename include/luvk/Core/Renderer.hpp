// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/Extensions.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <Volk/volk.h>

namespace luvk
{

    /** Render module index representation */
    enum class RenderModuleIndex : std::uint8_t
    {
        DEVICE = 0U,
        // ...

        Count
    };

    using ModuleIndex = std::underlying_type_t<RenderModuleIndex>;
    static constexpr inline std::uint8_t ToModuleIndex(RenderModuleIndex const Index) { return static_cast<ModuleIndex>(Index); }

    /** Renderer object that will be responsible for managing all resources and modules */
    class LUVKMODULE_API Renderer : public IRenderModule, public std::enable_shared_from_this<Renderer>
    {
        VkInstance m_Instance {};
        InstanceExtensions m_Extensions {};

        /** Modules:
         * 1. Device
         * 2. ...
         */
        Array<std::shared_ptr<IRenderModule>, ToModuleIndex(RenderModuleIndex::Count)> m_RenderModules{};

    public:
        constexpr Renderer() = default;
        ~Renderer() override = default;

        /** Pre initialize the renderer, loading the volk library, fetching available extensions and other resources */
        void PreInitializeRenderer();

        /** Post initialize the renderer, setting up the direct dependencies of this module such as device module, etc. */
        void PostInitializeRenderer();

        /** Get associated vulkan instance */
        [[nodiscard]] inline VkInstance const& GetInstance() const
        {
            return m_Instance;
        }

        /** Get the available instance layers and extensions */
        [[nodiscard]] inline InstanceExtensions& GetExtensions()
        {
            return m_Extensions;
        }

        /** Get the render module in the specified index */
        [[nodiscard]] inline std::shared_ptr<IRenderModule> const& GetModule(RenderModuleIndex const Index) const
        {
            return m_RenderModules.Data.at(ToModuleIndex(Index));
        }

        /** Arguments to create the vulkan instance */
        struct InstanceCreationArguments
        {
            std::string_view ApplicationName;
            std::string_view EngineName;
            std::uint32_t ApplicationVersion;
            std::uint32_t EngineVersion;
        };

        /** Initialize instance resources */
        [[nodiscard]] bool InitializeRenderer(InstanceCreationArguments const& Arguments);

    private:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;
    };
} // namespace luvk