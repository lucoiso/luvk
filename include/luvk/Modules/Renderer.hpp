// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include "luvk/Interfaces/IEventModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Modules/Synchronization.hpp"
#include "luvk/Resources/Extensions.hpp"

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
    class Draw;

    enum class RendererEvents : std::uint8_t
    {
        OnModulesRegistered,
        OnInitialized,
        OnRefreshed,
        OnPaused,
        OnResumed,
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
        std::shared_ptr<Draw>            DrawModule{nullptr};

        std::vector<std::shared_ptr<IRenderModule>> ExtraModules{};
    };

    class LUVK_API Renderer : public IRenderModule,
                              public IEventModule
    {
    protected:
        bool               m_Paused{false};
        VkInstance         m_Instance{VK_NULL_HANDLE};
        InstanceExtensions m_Extensions{};
        RenderModules      m_Modules{};

    public:
        constexpr Renderer() = default;

        ~Renderer() override
        {
            Renderer::ClearResources();
        }

        [[nodiscard]] constexpr VkInstance GetInstance() const noexcept
        {
            return m_Instance;
        }

        [[nodiscard]] constexpr InstanceExtensions& GetExtensions() noexcept
        {
            return m_Extensions;
        }

        [[nodiscard]] RenderModules GetModules() const noexcept
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
        void               DrawFrame() const;
        void               SetPaused(bool Paused);
        void               Refresh(const VkExtent2D& Extent) const;

    protected:
        void ClearResources() override;
    };
} // namespace luvk
