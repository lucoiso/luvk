// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Modules/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Subsystems/Memory.hpp"
#include "luvk/Subsystems/Synchronization.hpp"
#include "luvk/Subsystems/Extensions.hpp"

#include <vector>
#include <memory>
#include <functional>
#include <span>

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
        [[nodiscard]] VkInstance const& GetInstance() const
        {
            return m_Instance;
        }

        /** Get the available instance layers and extensions */
        [[nodiscard]] InstanceExtensions& GetExtensions()
        {
            return m_Extensions;
        }

        /** Get the render modules */
        [[nodiscard]] std::vector<std::shared_ptr<IRenderModule>> const& GetModules() const
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
        [[nodiscard]] constexpr std::shared_ptr<Type> FindModule() const
        {
            for (const std::shared_ptr<IRenderModule>& ModuleIt : m_RenderModules)
            {
                if (const std::shared_ptr<Type> CastTarget = std::dynamic_pointer_cast<Type>(ModuleIt))
                {
                    return CastTarget;
                }
            }

            return nullptr;
        }

        /** Pre initialize the renderer, loading the volk library, fetching available extensions and other resources */
        void PreInitializeRenderer();

        /** Register render modules before initialization */
        void RegisterModules(std::vector<std::shared_ptr<IRenderModule>>&& Modules);

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
        void InitializeRenderLoop();

        /** Draw a single frame */
        void DrawFrame();

        /** Pause or resume rendering */
        void SetPaused(bool Paused);

        /** Recreate swap chain resources with new extent */
        void Refresh(const VkExtent2D& Extent) const;

        /** Enqueue a command to be executed after the graphics pass */
        void EnqueueCommand(std::function<void(VkCommandBuffer)>&& Cmd);

        /** Enqueue a destructor for external resources */
        void EnqueueDestructor(std::function<void()>&& Destructor);

        /** Create an external descriptor pool */
        [[nodiscard]] VkDescriptorPool CreateExternalDescriptorPool(std::uint32_t MaxSets,
                                                                    std::span<VkDescriptorPoolSize const> PoolSizes,
                                                                    VkDescriptorPoolCreateFlags Flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

        /** Destroy a previously created external descriptor pool */
        void DestroyExternalDescriptorPool(VkDescriptorPool& Pool) const;

        /** Called before recording external commands */
        void BeginExternalFrame();

        /** Called after submitting external commands */
        void EndExternalFrame();

    private:
        /** Indicates if rendering is paused */
        bool m_Paused{false};

        /** Queue of commands to execute while the render pass is active */
        std::vector<std::function<void(VkCommandBuffer)>> m_PostRenderCommands{};

        /** Destructors for external resources */
        std::vector<std::function<void()>> m_ExternalDestructors{};

        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources() override;
        //~ End of IRenderModule interface

        /** Prepare per-frame resources */
        void SetupFrames() const;

        /** Record compute pass commands */
        void RecordComputePass(const VkCommandBuffer& Cmd) const;

        /** Record graphics and mesh pass commands */
        void RecordGraphicsPass(luvk::Synchronization::FrameData& Frame, std::uint32_t ImageIndex);

        /** Record rendering commands into command buffers */
        void RecordCommands(luvk::Synchronization::FrameData& Frame, std::uint32_t ImageIndex);

        /** Submit recorded commands to the graphics queue */
        void SubmitFrame(luvk::Synchronization::FrameData& Frame, std::uint32_t ImageIndex) const;
    };
} // namespace luvk
