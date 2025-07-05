// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/IRenderModule.hpp"
#include "luvk/Core/ThreadPool.hpp"
#include "luvk/Core/CommandPool.hpp"
#include "luvk/Core/SwapChain.hpp"
#include "luvk/Core/MeshRegistry.hpp"
#include "luvk/Core/Device.hpp"
#include "luvk/Core/Synchronization.hpp"
#include "luvk/Core/Extensions.hpp"

#include <vector>
#include <memory>
#include <optional>

namespace luvk
{
    /** Renderer Event Keys */
    enum class RendererEvents : std::uint8_t
    {
        OnPostInitialized, OnRenderLoopInitialized
    };

    /** Renderer object that will be responsible for managing all resources and modules */
    class LUVKMODULE_API Renderer : public IRenderModule,
                                    public std::enable_shared_from_this<Renderer>
    {
        /** Created Vulkan instance */
        VkInstance m_Instance{VK_NULL_HANDLE};

        /** Extensions available for the instance */
        InstanceExtensions m_Extensions{};

        /** Modules registered to the renderer */
        std::vector<std::shared_ptr<IRenderModule>> m_RenderModules{};

    public:
        constexpr Renderer() = default;

        //~ Begin of IRenderModule interface
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

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const& DeviceModule) const noexcept override
        {
            return nullptr;
        }

        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

        /** Find module from type */
        template <typename Type>
        [[nodiscard]] constexpr Type* FindModule()
        {
            for (const std::shared_ptr<IRenderModule>& ModuleIt : m_RenderModules)
            {
                if (Type* const CastTarget = dynamic_cast<Type*>(ModuleIt.get()))
                {
                    return CastTarget;
                }
            }

            return nullptr;
        }

        /** Find module from type (const version) */
        template <typename Type>
        [[nodiscard]] constexpr Type const* FindModule() const
        {
            for (const std::shared_ptr<IRenderModule>& ModuleIt : m_RenderModules)
            {
                if (Type const* const CastTarget = dynamic_cast<Type const*>(ModuleIt.get()))
                {
                    return CastTarget;
                }
            }

            return nullptr;
        }

        /** Pre initialize the renderer, loading the volk library, fetching available extensions and other resources */
        void PreInitializeRenderer();

        /** Register render modules before initialization */
        void RegisterModules(std::vector<std::shared_ptr<IRenderModule>> Modules);

        /** Arguments to create the vulkan instance */
        struct InstanceCreationArguments
        {
            std::string_view ApplicationName;
            std::string_view EngineName;
            std::uint32_t ApplicationVersion;
            std::uint32_t EngineVersion;
        };

        /** Initialize instance resources */
        [[nodiscard]] bool InitializeRenderer(InstanceCreationArguments const& Arguments, void const* pNext);

        /** Post initialize the renderer, setting up submodules of the renderer */
        void PostInitializeRenderer();

        /** Initialize per-frame rendering resources */
        void InitializeRenderLoop(std::shared_ptr<Device> const& Device,
                                  std::shared_ptr<SwapChain> const& SwapChain,
                                  std::shared_ptr<CommandPool> const& CommandPool,
                                  std::shared_ptr<MeshRegistry> const& MeshRegistry,
                                  std::shared_ptr<ThreadPool> const& ThreadPool);

        /** Draw a single frame */
        void DrawFrame();

        /** Pause or resume rendering */
        void SetPaused(bool Paused);

        /** Recreate swap chain resources with new extent */
        void Refresh(VkExtent2D Extent);

        struct RenderTargets
        {
            std::vector<VkImageView> ColorViews{};
            std::vector<VkFormat> ColorFormats{};
            VkExtent2D Extent{0, 0};
        };

        /** Override rendering targets. Empty to restore swapchain rendering */
        void SetRenderTargets(RenderTargets Targets);

    private: /** Indicates if rendering is paused */
        bool m_Paused{false};

        /** Device module used for resource creation */
        std::shared_ptr<Device> m_DeviceModule{};

        /** Swapchain module managing presentation */
        std::shared_ptr<SwapChain> m_SwapChainModule{};

        /** Command pool for frame submission */
        std::shared_ptr<CommandPool> m_CommandPoolModule{};

        /** Registry of loaded meshes */
        std::shared_ptr<MeshRegistry> m_MeshRegistryModule{};

        /** Thread pool used for asynchronous tasks */
        std::shared_ptr<ThreadPool> m_ThreadPoolModule{};

        /** Custom render targets used instead of the swapchain */
        std::optional<RenderTargets> m_CustomTargets{};

        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources() override;
        //~ End of IRenderModule interface

        /** Prepare per-frame resources */
        void SetupFrames();

        /** Record rendering commands into command buffers */
        void RecordCommands(luvk::Synchronization::FrameData& Frame, std::uint32_t ImageIndex);

        /** Submit recorded commands to the graphics queue */
        void SubmitFrame(luvk::Synchronization::FrameData& Frame, std::uint32_t ImageIndex);
    };
} // namespace luvk
