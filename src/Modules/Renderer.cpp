// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Renderer.hpp"
#include <algorithm>
#include <execution>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/SwapChain.hpp"

static constinit std::atomic_bool s_IsVolkInitialized = false;

void luvk::Renderer::RegisterModules(Vector<std::shared_ptr<IRenderModule>>&& Modules)
{
    if (!s_IsVolkInitialized.load())
    {
        volkInitialize();
        s_IsVolkInitialized.store(true);
    }

    if (m_Extensions.IsEmpty())
    {
        m_Extensions.FillExtensionsContainer();
    }

    m_ModuleMap.clear();
    for (const auto& ModuleIt : Modules)
    {
        m_ModuleMap.emplace(std::type_index(typeid(*ModuleIt)), ModuleIt);
    }
    GetEventSystem().Execute(RendererEvents::OnModulesRegistered);
}

bool luvk::Renderer::InitializeRenderer(const InstanceCreationArguments& Arguments, const void* pNext)
{
    if (std::empty(m_ModuleMap))
    {
        throw std::runtime_error("No modules were registered");
    }

    const void* FeatureChain = pNext;
    for (const auto& [Index, Module] : m_ModuleMap)
    {
        if (const auto* const ExtModule = dynamic_cast<const IExtensionsModule*>(Module.get()))
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

        if (const auto* const FeatChainModule = dynamic_cast<const IFeatureChainModule*>(Module.get()))
        {
            if (const auto Chain = FeatChainModule->GetInstanceFeatureChain())
            {
                const auto Base = const_cast<VkBaseInStructure*>(static_cast<const VkBaseInStructure*>(Chain));
                Base->pNext     = static_cast<const VkBaseInStructure*>(FeatureChain);
                FeatureChain    = Chain;
            }
        }
    }

    const VkApplicationInfo AppInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName = std::data(Arguments.ApplicationName),
                                    .applicationVersion = Arguments.ApplicationVersion,
                                    .pEngineName = std::data(Arguments.EngineName),
                                    .engineVersion = Arguments.EngineVersion,
                                    .apiVersion = VK_API_VERSION_1_4};

    const auto Layers     = m_Extensions.GetEnabledLayersNames();
    const auto Extensions = m_Extensions.GetEnabledExtensionsNames();

    const VkInstanceCreateInfo InstanceCreateInfo{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                  .pNext = FeatureChain,
                                                  .pApplicationInfo = &AppInfo,
                                                  .enabledLayerCount = static_cast<std::uint32_t>(std::size(Layers)),
                                                  .ppEnabledLayerNames = std::data(Layers),
                                                  .enabledExtensionCount = static_cast<std::uint32_t>(std::size(Extensions)),
                                                  .ppEnabledExtensionNames = std::data(Extensions)};

    vkCreateInstance(&InstanceCreateInfo, nullptr, &m_Instance);

    if (m_Instance != nullptr)
    {
        volkLoadInstance(m_Instance);
        std::for_each(std::execution::seq,
                      std::begin(m_ModuleMap),
                      std::end(m_ModuleMap),
                      [&](const auto& Pair)
                      {
                          Pair.Second->InitializeResources();
                      });
        GetEventSystem().Execute(RendererEvents::OnInitialized);
        return true;
    }
    return false;
}

void luvk::Renderer::ClearResources()
{
    const auto DeviceModule = FindModule<Device>();
    if (const VkDevice& LogicalDevice = DeviceModule->GetLogicalDevice();
        LogicalDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(LogicalDevice);
    }

    std::for_each(std::execution::seq,
                  std::begin(m_ModuleMap),
                  std::end(m_ModuleMap),
                  [](auto& Pair)
                  {
                      Pair.Second.reset();
                  });
    m_ModuleMap.clear();

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }

    if (s_IsVolkInitialized.load())
    {
        volkFinalize();
        s_IsVolkInitialized.store(false);
    }
}

void luvk::Renderer::InitializeRenderLoop() const
{
    SetupFrames();
    GetEventSystem().Execute(RendererEvents::OnRenderLoopInitialized);
}

void luvk::Renderer::DrawFrame()
{
    if (m_Paused)
    {
        return;
    }

    const auto      DeviceModule    = FindModule<Device>();
    const auto      SwapChainModule = FindModule<SwapChain>();
    const VkDevice& LogicalDevice   = DeviceModule->GetLogicalDevice();

    const auto Sync = FindModule<Synchronization>();

    auto& Frame = Sync->GetFrame(Sync->GetCurrentFrame());

    if (Frame.Submitted)
    {
        DeviceModule->Wait(Frame.InFlight, VK_TRUE, UINT64_MAX);
        vkResetFences(LogicalDevice, 1, &Frame.InFlight);
        Frame.Submitted = false;
    }

    std::uint32_t ImageIndex = 0;

    const VkResult AcquireResult = vkAcquireNextImageKHR(LogicalDevice,
                                                         SwapChainModule->GetHandle(),
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

    if (!LUVK_EXECUTE(vkResetCommandBuffer(Frame.CommandBuffer, 0)))
    {
        throw std::runtime_error("Failed to reset command buffer.");
    }

    RecordCommands(Frame, ImageIndex);
    SubmitFrame(Frame, ImageIndex);
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
    const auto DeviceModule    = FindModule<Device>();
    const auto SwapChainModule = FindModule<SwapChain>();

    DeviceModule->WaitIdle();
    SwapChainModule->Recreate(Extent, nullptr);

    SetupFrames();
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
    FindModule<Synchronization>()->SetupFrames();
}

void luvk::Renderer::RecordCommands(const Synchronization::FrameData& Frame, const std::uint32_t ImageIndex)
{
    constexpr VkCommandBufferBeginInfo Begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};

    LUVK_EXECUTE(vkBeginCommandBuffer(Frame.CommandBuffer, &Begin));

    if (m_PreRenderCallback)
    {
        m_PreRenderCallback(Frame.CommandBuffer);
    }

    const auto       SwapChainModule = FindModule<SwapChain>();
    const VkExtent2D Extent          = SwapChainModule->GetExtent();

    constexpr luvk::Array       Clear{VkClearValue{.color = {0.1F, 0.1F, 0.1F, 1.F}}, VkClearValue{.depthStencil = {1.F, 0}}};
    const VkRenderPassBeginInfo BeginPass{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                          .renderPass = SwapChainModule->GetRenderPass(),
                                          .framebuffer = SwapChainModule->GetFramebuffer(ImageIndex),
                                          .renderArea = {{0, 0}, {Extent.width, Extent.height}},
                                          .clearValueCount = static_cast<std::uint32_t>(std::size(Clear)),
                                          .pClearValues = std::data(Clear)};

    vkCmdBeginRenderPass(Frame.CommandBuffer, &BeginPass, VK_SUBPASS_CONTENTS_INLINE);

    const VkViewport Viewport{0.F, 0.F, static_cast<float>(Extent.width), static_cast<float>(Extent.height), 0.F, 1.F};
    const VkRect2D   Scissor{{0, 0}, Extent};

    vkCmdSetViewport(Frame.CommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(Frame.CommandBuffer, 0, 1, &Scissor);

    if (m_DrawCallback)
    {
        m_DrawCallback(Frame.CommandBuffer);
    }

    for (const auto& Cmd : m_PostRenderCommands)
    {
        Cmd(Frame.CommandBuffer);
    }
    m_PostRenderCommands.clear();

    vkCmdEndRenderPass(Frame.CommandBuffer);
    LUVK_EXECUTE(vkEndCommandBuffer(Frame.CommandBuffer));
}

void luvk::Renderer::SubmitFrame(Synchronization::FrameData& Frame, const std::uint32_t ImageIndex) const
{
    const auto SyncModule      = FindModule<Synchronization>();
    const auto DeviceModule    = FindModule<Device>();
    const auto SwapChainModule = FindModule<SwapChain>();

    constexpr VkPipelineStageFlags WaitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    const VkSubmitInfo Submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .waitSemaphoreCount = 1,
                              .pWaitSemaphores = &Frame.ImageAvailable,
                              .pWaitDstStageMask = &WaitStages,
                              .commandBufferCount = 1,
                              .pCommandBuffers = &Frame.CommandBuffer,
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores = &SyncModule->GetRenderFinished(ImageIndex)};

    const VkQueue GraphicsQueue = DeviceModule->GetQueue(DeviceModule->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value());

    LUVK_EXECUTE(vkQueueSubmit(GraphicsQueue, 1, &Submit, Frame.InFlight));
    Frame.Submitted = true;

    const VkSwapchainKHR&  Handle = SwapChainModule->GetHandle();
    const VkPresentInfoKHR Present{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                   .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = &SyncModule->GetRenderFinished(ImageIndex),
                                   .swapchainCount = 1,
                                   .pSwapchains = &Handle,
                                   .pImageIndices = &ImageIndex};

    if (const VkResult PresentResult = vkQueuePresentKHR(GraphicsQueue, &Present);
        PresentResult == VK_ERROR_OUT_OF_DATE_KHR || PresentResult == VK_SUBOPTIMAL_KHR) {}
    else if (PresentResult != VK_SUCCESS)
    {
        throw std::runtime_error("Present failed");
    }

    SyncModule->AdvanceFrame();
}
