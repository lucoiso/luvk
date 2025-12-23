// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

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
    if (m_Extensions.IsEmpty() == true)
    {
        m_Extensions.FillExtensionsContainer();
    }

    m_Modules = std::move(Modules);

    GetEventSystem().Execute(RendererEvents::OnModulesRegistered);
}

bool luvk::Renderer::InitializeRenderer(const InstanceCreationArguments& Arguments, const void* pNext)
{
    volkInitialize();
    
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

    const VkApplicationInfo AppInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName = std::data(Arguments.ApplicationName),
                                    .applicationVersion = Arguments.ApplicationVersion,
                                    .pEngineName = std::data(Arguments.EngineName),
                                    .engineVersion = Arguments.EngineVersion,
                                    .apiVersion = VK_API_VERSION_1_4};

    const std::vector<const char*> Layers     = m_Extensions.GetEnabledLayersNames();
    const std::vector<const char*> Extensions = m_Extensions.GetEnabledExtensionsNames();

    const VkInstanceCreateInfo InstanceCreateInfo{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                  .pNext = FeatureChain,
                                                  .pApplicationInfo = &AppInfo,
                                                  .enabledLayerCount = static_cast<std::uint32_t>(std::size(Layers)),
                                                  .ppEnabledLayerNames = std::data(Layers),
                                                  .enabledExtensionCount = static_cast<std::uint32_t>(std::size(Extensions)),
                                                  .ppEnabledExtensionNames = std::data(Extensions)};

    if (LUVK_EXECUTE(vkCreateInstance(&InstanceCreateInfo, nullptr, &m_Instance)) == false)
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

    volkFinalize();
}

void luvk::Renderer::DrawFrame() const
{
    if (m_Paused == true)
    {
        return;
    }

    const VkDevice LogicalDevice = m_Modules.DeviceModule->GetLogicalDevice();
    FrameData&     Frame         = m_Modules.SynchronizationModule->GetFrame(m_Modules.SynchronizationModule->GetCurrentFrame());

    if (Frame.Submitted == true)
    {
        m_Modules.DeviceModule->Wait(Frame.InFlight, VK_TRUE, UINT64_MAX);
        vkResetFences(LogicalDevice, 1, &Frame.InFlight);
        Frame.Submitted = false;
    }

    std::uint32_t  ImageIndex    = 0U;
    const VkResult AcquireResult = vkAcquireNextImageKHR(LogicalDevice,
                                                         m_Modules.SwapChainModule->GetHandle(),
                                                         UINT64_MAX,
                                                         Frame.ImageAvailable,
                                                         VK_NULL_HANDLE,
                                                         &ImageIndex);

    if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        return;
    }

    if (AcquireResult != VK_SUCCESS && AcquireResult != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swap chain image.");
    }

    if (LUVK_EXECUTE(vkResetCommandBuffer(Frame.CommandBuffer, 0U)) == false)
    {
        throw std::runtime_error("Failed to reset command buffer.");
    }

    m_Modules.DrawModule->RecordCommands(Frame, ImageIndex);
    m_Modules.DrawModule->SubmitFrame(Frame, ImageIndex);
}

void luvk::Renderer::SetPaused(const bool Paused)
{
    m_Paused = Paused;
    GetEventSystem().Execute(m_Paused
                                 ? RendererEvents::OnPaused
                                 : RendererEvents::OnResumed);
}

void luvk::Renderer::Refresh(const VkExtent2D& Extent) const
{
    m_Modules.DeviceModule->WaitIdle();
    m_Modules.SwapChainModule->Recreate(Extent, nullptr);
    m_Modules.SynchronizationModule->SetupFrames();
    GetEventSystem().Execute(RendererEvents::OnRefreshed);
}
