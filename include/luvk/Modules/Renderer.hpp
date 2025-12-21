// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <functional>
#include <memory>
#include "luvk/Module.hpp"
#include "luvk/Interfaces/IEventModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Modules/Synchronization.hpp"
#include "luvk/Resources/Extensions.hpp"
#include "luvk/Types/Vector.hpp"
#include "luvk/Types/Array.hpp"

namespace luvk
{
    class Debug;
    class Device;
    class Memory;
    class SwapChain;
    class CommandPool;
    class Synchronization;
    class ThreadPool;
    class DescriptorPool;

    enum class RendererEvents : std::uint8_t
    {
        OnModulesRegistered,
        OnInitialized,
        OnRefreshed,
        OnPaused,
        OnResumed,
        OnRenderLoopInitialized
    };

    struct RenderModules
    {
        std::shared_ptr<Debug>           DebugModule{nullptr};
        std::shared_ptr<Device>          DeviceModule{nullptr};
        std::shared_ptr<Memory>          MemoryModule{nullptr};
        std::shared_ptr<SwapChain>       SwapChainModule{nullptr};
        std::shared_ptr<CommandPool>     CommandPoolModule{nullptr};
        std::shared_ptr<Synchronization> SynchronizationModule{nullptr};
        std::shared_ptr<ThreadPool>      ThreadPoolModule{nullptr};
        std::shared_ptr<DescriptorPool>  DescriptorPoolModule{nullptr};

        Vector<std::shared_ptr<IRenderModule>> ExtraModules{};
    };

    class LUVKMODULE_API Renderer : public IRenderModule,
                                    public IEventModule
    {
    protected:
        bool                         m_Paused{false};
        VkInstance                   m_Instance{VK_NULL_HANDLE};
        InstanceExtensions           m_Extensions{};
        luvk::Array<VkClearValue, 2> m_ClearValues{VkClearValue{.color = {0.2F, 0.2F, 0.2F, 1.F}},
                                                   VkClearValue{.depthStencil = {1.F, 0}}};
        Vector<std::function<void(VkCommandBuffer)>> m_PostRenderCommands{};
        RenderModules                                m_Modules{};
        std::function<void(VkCommandBuffer)>         m_PreRenderCallback{nullptr};
        std::function<void(VkCommandBuffer)>         m_DrawCallback{nullptr};

    public:
        constexpr Renderer() = default;

        virtual ~Renderer() override
        {
            Renderer::ClearResources();
        }

        [[nodiscard]] const VkInstance& GetInstance() const
        {
            return m_Instance;
        }

        [[nodiscard]] constexpr InstanceExtensions& GetExtensions()
        {
            return m_Extensions;
        }

        [[nodiscard]] constexpr luvk::Array<VkClearValue, 2> GetClearValues() const
        {
            return m_ClearValues;
        }

        void SetClearValues(luvk::Array<VkClearValue, 2>&& Values);

        [[nodiscard]] constexpr const RenderModules& GetModules() const
        {
            return m_Modules;
        }

        void RegisterModules(RenderModules&& Modules);

        struct InstanceCreationArguments
        {
            std::string_view ApplicationName;
            std::string_view EngineName;
            std::uint32_t    ApplicationVersion;
            std::uint32_t    EngineVersion;
        };

        [[nodiscard]] bool InitializeRenderer(const InstanceCreationArguments& Arguments, const void* pNext);
        void               InitializeRenderLoop() const;
        void               DrawFrame();
        void               SetPaused(bool Paused);
        void               Refresh(const VkExtent2D& Extent) const;

        void EnqueueCommand(std::function<void(VkCommandBuffer)>&& Cmd);
        void SetPreRenderCallback(std::function<void(VkCommandBuffer)>&& Callback);
        void SetDrawCallback(std::function<void(VkCommandBuffer)>&& Callback);

    protected:
        virtual void ClearResources() override;

    private:
        void SetupFrames() const;
        void RecordCommands(Synchronization::FrameData& Frame, std::uint32_t ImageIndex);
        void SubmitFrame(Synchronization::FrameData& Frame, std::uint32_t ImageIndex) const;
    };
} // namespace luvk
