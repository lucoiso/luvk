// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Modules/Synchronization.hpp"
#include "luvk/Resources/Extensions.hpp"

#include <vector>
#include <memory>
#include <functional>
#include <span>
#include <unordered_map>
#include <typeindex>

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

        /** Fast lookup table of registered modules */
        std::unordered_map<std::type_index, std::shared_ptr<IRenderModule>> m_ModuleMap{};

        /** Extensions available for the instance */
        InstanceExtensions m_Extensions{};

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
        [[nodiscard]] auto const& GetModules() const
        {
            return m_ModuleMap;
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
            const auto It = m_ModuleMap.find(std::type_index(typeid(Type)));
            if (It != std::end(m_ModuleMap))
            {
                return std::static_pointer_cast<Type>(It->second);
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
        /** Queue of commands to execute while the render pass is active */
        std::vector<std::function<void(VkCommandBuffer)>> m_PostRenderCommands{};

        /** Destructors for external resources */
        std::vector<std::function<void()>> m_ExternalDestructors{};

        /** Indicates if rendering is paused */
        bool m_Paused{false};

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
