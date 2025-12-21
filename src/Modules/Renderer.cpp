// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Renderer.hpp"
#include <algorithm>
#include <execution>
#include <atomic>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Debug.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Modules/SwapChain.hpp"
#include "luvk/Modules/CommandPool.hpp"
#include "luvk/Modules/Synchronization.hpp"
#include "luvk/Modules/ThreadPool.hpp"
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IFeatureChainModule.hpp"

static constinit std::atomic_bool s_IsVolkInitialized = false;

void luvk::Renderer::SetClearValues(luvk::Array<VkClearValue, 2>&& Values)
{
    m_ClearValues = Values;
}

void luvk::Renderer::RegisterModules(RenderModules&& Modules)
{
    if (s_IsVolkInitialized.load() == false)
    {
        volkInitialize();
        s_IsVolkInitialized.store(true);
    }

    if (m_Extensions.IsEmpty() == true)
    {
        m_Extensions.FillExtensionsContainer();
    }

    m_Modules = std::move(Modules);

    GetEventSystem().Execute(RendererEvents::OnModulesRegistered);
}

bool luvk::Renderer::InitializeRenderer(const InstanceCreationArguments& Arguments, const void* pNext)
{
    Vector<std::shared_ptr<IRenderModule>> AllModules;
    AllModules.push_back(m_Modules.DebugModule);
    AllModules.push_back(m_Modules.DeviceModule);
    AllModules.push_back(m_Modules.MemoryModule);
    AllModules.push_back(m_Modules.SwapChainModule);
    AllModules.push_back(m_Modules.CommandPoolModule);
    AllModules.push_back(m_Modules.SynchronizationModule);
    AllModules.push_back(m_Modules.ThreadPoolModule);
    AllModules.push_back(m_Modules.DescriptorPoolModule);
    AllModules.insert(std::end(AllModules), std::begin(m_Modules.ExtraModules), std::end(m_Modules.ExtraModules));

    const void* FeatureChain = pNext;

    for (const std::shared_ptr<IRenderModule>& Module : AllModules)
    {
        if (Module == nullptr)
        {
            continue;
        }

        if (const IExtensionsModule* const ExtModule = dynamic_cast<const IExtensionsModule*>(Module.get());
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

        if (const IFeatureChainModule* const FeatChainModule = dynamic_cast<const IFeatureChainModule*>(Module.get());
            FeatChainModule != nullptr)
        {
            if (const void* const Chain = FeatChainModule->GetInstanceFeatureChain();
                Chain != nullptr)
            {
                VkBaseInStructure* const Base = const_cast<VkBaseInStructure*>(static_cast<const VkBaseInStructure*>(Chain));
                Base->pNext                   = static_cast<const VkBaseInStructure*>(FeatureChain);
                FeatureChain                  = Chain;
            }
        }
    }

    const VkApplicationInfo AppInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = std::data(Arguments.ApplicationName),
        .applicationVersion = Arguments.ApplicationVersion,
        .pEngineName = std::data(Arguments.EngineName),
        .engineVersion = Arguments.EngineVersion,
        .apiVersion = VK_API_VERSION_1_4
    };

    const Vector<const char*> Layers     = m_Extensions.GetEnabledLayersNames();
    const Vector<const char*> Extensions = m_Extensions.GetEnabledExtensionsNames();

    const VkInstanceCreateInfo InstanceCreateInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = FeatureChain,
        .pApplicationInfo = &AppInfo,
        .enabledLayerCount = static_cast<std::uint32_t>(std::size(Layers)),
        .ppEnabledLayerNames = std::data(Layers),
        .enabledExtensionCount = static_cast<std::uint32_t>(std::size(Extensions)),
        .ppEnabledExtensionNames = std::data(Extensions)
    };

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
    if (m_Modules.DeviceModule != nullptr)
    {
        if (const VkDevice& LogicalDevice = m_Modules.DeviceModule->GetLogicalDevice();
            LogicalDevice != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(LogicalDevice);
        }
    }

    m_Modules.DebugModule.reset();
    m_Modules.DeviceModule.reset();
    m_Modules.MemoryModule.reset();
    m_Modules.SwapChainModule.reset();
    m_Modules.CommandPoolModule.reset();
    m_Modules.SynchronizationModule.reset();
    m_Modules.ThreadPoolModule.reset();
    m_Modules.DescriptorPoolModule.reset();
    m_Modules.ExtraModules.clear();

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }

    if (s_IsVolkInitialized.load() == true)
    {
        volkFinalize();
        s_IsVolkInitialized.store(false);
    }
}

void luvk::Renderer::InitializeRenderLoop() const
{
    Renderer::SetupFrames();
    GetEventSystem().Execute(RendererEvents::OnRenderLoopInitialized);
}

void luvk::Renderer::DrawFrame()
{
    if (m_Paused == true)
    {
        return;
    }

    const VkDevice&             LogicalDevice = m_Modules.DeviceModule->GetLogicalDevice();
    Synchronization::FrameData& Frame         = m_Modules.SynchronizationModule->GetFrame(m_Modules.SynchronizationModule->GetCurrentFrame());

    if (Frame.Submitted == true)
    {
        m_Modules.DeviceModule->Wait(Frame.InFlight, VK_TRUE, UINT64_MAX);
        vkResetFences(LogicalDevice, 1, &Frame.InFlight);
        Frame.Submitted = false;
    }

    std::uint32_t  ImageIndex    = 0U;
    const VkResult AcquireResult = vkAcquireNextImageKHR(
                                                         LogicalDevice,
                                                         m_Modules.SwapChainModule->GetHandle(),
                                                         UINT64_MAX,
                                                         Frame.ImageAvailable,
                                                         VK_NULL_HANDLE,
                                                         &ImageIndex
                                                        );

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

    Renderer::RecordCommands(Frame, ImageIndex);
    Renderer::SubmitFrame(Frame, ImageIndex);
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

    Renderer::SetupFrames();
    GetEventSystem().Execute(RendererEvents::OnRefreshed);
}

void luvk::Renderer::EnqueueCommand(std::function<void(VkCommandBuffer)>&& Cmd)
{
    m_PostRenderCommands.emplace_back(std::move(Cmd));
}

void luvk::Renderer::SetPreRenderCallback(std::function<void(VkCommandBuffer)>&& Callback)
{
    m_PreRenderCallback = std::move(Callback);
}

void luvk::Renderer::SetDrawCallback(std::function<void(VkCommandBuffer)>&& Callback)
{
    m_DrawCallback = std::move(Callback);
}

void luvk::Renderer::SetupFrames() const
{
    m_Modules.SynchronizationModule->SetupFrames();
}

void luvk::Renderer::RecordCommands(Synchronization::FrameData& Frame, const std::uint32_t ImageIndex)
{
    constexpr VkCommandBufferBeginInfo Begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    LUVK_EXECUTE(vkBeginCommandBuffer(Frame.CommandBuffer, &Begin));

    if (m_PreRenderCallback != nullptr)
    {
        m_PreRenderCallback(Frame.CommandBuffer);
    }

    const VkExtent2D& Extent = m_Modules.SwapChainModule->GetExtent();

    const VkRenderPassBeginInfo BeginPass{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = m_Modules.SwapChainModule->GetRenderPass(),
        .framebuffer = m_Modules.SwapChainModule->GetFramebuffer(ImageIndex),
        .renderArea = {{0, 0}, {Extent.width, Extent.height}},
        .clearValueCount = static_cast<std::uint32_t>(std::size(m_ClearValues)),
        .pClearValues = std::data(m_ClearValues)
    };

    vkCmdBeginRenderPass(Frame.CommandBuffer, &BeginPass, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport Viewport{0.F, 0.F, static_cast<float>(Extent.width), static_cast<float>(Extent.height), 0.F, 1.F};
    const VkRect2D   Scissor{{0, 0}, Extent};

    vkCmdSetViewport(Frame.CommandBuffer, 0U, 1U, &Viewport);
    vkCmdSetScissor(Frame.CommandBuffer, 0U, 1U, &Scissor);

    if (m_DrawCallback != nullptr)
    {
        m_DrawCallback(Frame.CommandBuffer);
    }

    for (const std::function<void(VkCommandBuffer)>& Cmd : m_PostRenderCommands)
    {
        Cmd(Frame.CommandBuffer);
    }
    m_PostRenderCommands.clear();

    vkCmdEndRenderPass(Frame.CommandBuffer);
    LUVK_EXECUTE(vkEndCommandBuffer(Frame.CommandBuffer));
}

void luvk::Renderer::SubmitFrame(Synchronization::FrameData& Frame, const std::uint32_t ImageIndex) const
{
    constexpr VkPipelineStageFlags WaitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    const VkSubmitInfo Submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1U,
        .pWaitSemaphores = &Frame.ImageAvailable,
        .pWaitDstStageMask = &WaitStages,
        .commandBufferCount = 1U,
        .pCommandBuffers = &Frame.CommandBuffer,
        .signalSemaphoreCount = 1U,
        .pSignalSemaphores = &m_Modules.SynchronizationModule->GetRenderFinished(ImageIndex)
    };

    const VkQueue GraphicsQueue = m_Modules.DeviceModule->GetQueue(m_Modules.DeviceModule->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value());

    LUVK_EXECUTE(vkQueueSubmit(GraphicsQueue, 1U, &Submit, Frame.InFlight));
    Frame.Submitted = true;

    const VkSwapchainKHR&  Handle = m_Modules.SwapChainModule->GetHandle();
    const VkPresentInfoKHR Present{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1U,
        .pWaitSemaphores = &m_Modules.SynchronizationModule->GetRenderFinished(ImageIndex),
        .swapchainCount = 1U,
        .pSwapchains = &Handle,
        .pImageIndices = &ImageIndex
    };

    const VkResult PresentResult = vkQueuePresentKHR(GraphicsQueue, &Present);
    if (PresentResult != VK_SUCCESS && PresentResult != VK_SUBOPTIMAL_KHR && PresentResult != VK_ERROR_OUT_OF_DATE_KHR)
    {
        throw std::runtime_error("Present failed");
    }

    m_Modules.SynchronizationModule->AdvanceFrame();
}
