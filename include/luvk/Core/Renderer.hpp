// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/Extensions.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <volk.h>
#include <cstdint>

namespace luvk
{
    /** Renderer object that will be responsible for managing all resources and modules */
    class LUVKMODULE_API Renderer : public IRenderModule, public std::enable_shared_from_this<Renderer>
    {
        VkInstance m_Instance {};
        InstanceExtensions m_Extensions {};
        std::vector<std::shared_ptr<IRenderModule>> m_RenderModules{};

    public:
        constexpr Renderer() = default;
        ~Renderer() override;

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

        /** Get the render modules */
        [[nodiscard]] inline std::vector<std::shared_ptr<IRenderModule>> const& GetModules() const
        {
            return m_RenderModules;
        }

        /** Pre initialize the renderer, loading the volk library, fetching available extensions and other resources */
        void PreInitializeRenderer();

        /** Arguments to create the vulkan instance */
        struct InstanceCreationArguments
        {
            std::string_view ApplicationName;
            std::string_view EngineName;
            std::uint32_t ApplicationVersion;
            std::uint32_t EngineVersion;
        };

        /** Initialize instance resources */
        [[nodiscard]] bool InitializeRenderer(InstanceCreationArguments const& Arguments, void* const& pNext);

        /** Post initialize the renderer, setting up submodules of the renderer, in order of initialization */
        void PostInitializeRenderer(std::vector<std::shared_ptr<IRenderModule>> &&Modules);

    private:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources(IRenderModule* MainRenderer) override;
    };
} // namespace luvk