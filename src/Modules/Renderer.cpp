/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Renderer.hpp"
#include <algorithm>
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IFeatureChainModule.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/CommandPool.hpp"
#include "luvk/Modules/Debug.hpp"
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Draw.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Modules/SwapChain.hpp"
#include "luvk/Modules/Synchronization.hpp"
#include "luvk/Modules/ThreadPool.hpp"

void luvk::Renderer::RegisterModules(RenderModules&& Modules)
{
    volkInitialize();

    if (m_Extensions.IsEmpty() == true)
    {
        m_Extensions.FillExtensionsContainer();
    }

    m_Modules = std::move(Modules);

    GetEventSystem().Execute(RendererEvents::OnModulesRegistered);
}

bool luvk::Renderer::InitializeRenderer(const InstanceCreationArguments& Arguments, const void* pNext)
{
    m_InstanceCreationArguments = Arguments;

    std::vector<std::shared_ptr<IRenderModule>> AllModules{m_Modules.DebugModule,
                                                           m_Modules.DeviceModule,
                                                           m_Modules.MemoryModule,
                                                           m_Modules.SwapChainModule,
                                                           m_Modules.CommandPoolModule,
                                                           m_Modules.SynchronizationModule,
                                                           m_Modules.ThreadPoolModule,
                                                           m_Modules.DescriptorPoolModule,
                                                           m_Modules.DrawModule};

    AllModules.insert(std::end(AllModules), std::begin(m_Modules.ExtraModules), std::end(m_Modules.ExtraModules));

    const void* FeatureChain = pNext;

    for (const std::shared_ptr<IRenderModule>& Module : AllModules)
    {
        if (Module == nullptr)
        {
            continue;
        }

        if (const auto* const ExtModule = dynamic_cast<const IExtensionsModule*>(Module.get());
            ExtModule != nullptr)
        {
            for (const auto& [LayerIt, ExtensionsContainerIt] : ExtModule->GetInstanceExtensions())
            {
                m_Extensions.SetLayerState(LayerIt, true);
                for (const std::string_view ExtensionIt : ExtensionsContainerIt)
                {
                    m_Extensions.SetExtensionState(LayerIt, ExtensionIt, true);
                }
            }
        }

        if (const auto* const FeatChainModule = dynamic_cast<const IFeatureChainModule*>(Module.get());
            FeatChainModule != nullptr)
        {
            if (const void* const Chain = FeatChainModule->GetInstanceFeatureChain();
                Chain != nullptr)
            {
                auto* const Base = const_cast<VkBaseInStructure*>(static_cast<const VkBaseInStructure*>(Chain));
                Base->pNext      = static_cast<const VkBaseInStructure*>(FeatureChain);
                FeatureChain     = Chain;
            }
        }
    }

    const VkApplicationInfo AppInfo{.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName   = std::data(Arguments.ApplicationName),
                                    .applicationVersion = Arguments.ApplicationVersion,
                                    .pEngineName        = std::data(Arguments.EngineName),
                                    .engineVersion      = Arguments.EngineVersion,
                                    .apiVersion         = Arguments.VulkanApiVersion};

    const std::vector<const char*> Layers     = m_Extensions.GetEnabledLayersNames();
    const std::vector<const char*> Extensions = m_Extensions.GetEnabledExtensionsNames();

    const VkInstanceCreateInfo InstanceCreateInfo{.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                  .pNext                   = FeatureChain,
                                                  .pApplicationInfo        = &AppInfo,
                                                  .enabledLayerCount       = static_cast<std::uint32_t>(std::size(Layers)),
                                                  .ppEnabledLayerNames     = std::data(Layers),
                                                  .enabledExtensionCount   = static_cast<std::uint32_t>(std::size(Extensions)),
                                                  .ppEnabledExtensionNames = std::data(Extensions)};

    if (!LUVK_EXECUTE(vkCreateInstance(&InstanceCreateInfo, nullptr, &m_Instance)))
    {
        return false;
    }

    if (m_Instance != nullptr)
    {
        volkLoadInstance(m_Instance);

        for (const std::shared_ptr<IRenderModule>& Module : AllModules)
        {
            if (Module != nullptr)
            {
                Module->InitializeResources();
            }
        }

        GetEventSystem().Execute(RendererEvents::OnInitialized);
        return true;
    }

    return false;
}

void luvk::Renderer::ClearResources()
{
    m_Modules.DebugModule.reset();
    m_Modules.DeviceModule.reset();
    m_Modules.MemoryModule.reset();
    m_Modules.SwapChainModule.reset();
    m_Modules.CommandPoolModule.reset();
    m_Modules.SynchronizationModule.reset();
    m_Modules.ThreadPoolModule.reset();
    m_Modules.DescriptorPoolModule.reset();
    m_Modules.DrawModule.reset();
    m_Modules.ExtraModules.clear();

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }

    GetEventSystem().Execute(RendererEvents::OnCleared);
    volkFinalize();
}

void luvk::Renderer::DrawFrame() const
{
    if (m_Paused == true)
    {
        return;
    }

    FrameData&                         Frame      = m_Modules.SynchronizationModule->GetCurrentFrameData();
    const std::optional<std::uint32_t> ImageIndex = m_Modules.SwapChainModule->Acquire(Frame);

    if (!ImageIndex.has_value())
    {
        return;
    }

    const std::uint32_t IndexValue = ImageIndex.value();

    m_Modules.DrawModule->RecordCommands(Frame, m_Modules.SwapChainModule->GetRenderTarget(IndexValue));
    m_Modules.DrawModule->SubmitFrame(Frame, IndexValue);
    m_Modules.SwapChainModule->Present(IndexValue);
}

void luvk::Renderer::SetPaused(const bool Paused)
{
    m_Paused = Paused;
    GetEventSystem().Execute(m_Paused ? RendererEvents::OnPaused : RendererEvents::OnResumed);
}

void luvk::Renderer::Refresh(const VkExtent2D& Extent) const
{
    m_Modules.DeviceModule->WaitQueue(VK_QUEUE_GRAPHICS_BIT);
    m_Modules.SwapChainModule->Recreate(Extent, nullptr);
    m_Modules.SynchronizationModule->ResetFrames();
    GetEventSystem().Execute(RendererEvents::OnRefreshed);
}
