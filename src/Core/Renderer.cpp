// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Device.hpp"
#include "luvk/Core/SwapChain.hpp"
#include "luvk/Core/CommandPool.hpp"
#include "luvk/Core/MeshRegistry.hpp"
#include "luvk/Core/ThreadPool.hpp"
#include "luvk/Types/MeshDraw.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include <execution>
#include <algorithm>
#include <limits>
#include <iterator>
#include <thread>
#include <future>

static bool s_IsVolkInitialized = false;

luvk::Renderer::~Renderer()
{
    Renderer::ClearResources();
}

void luvk::Renderer::PreInitializeRenderer()
{
    if (!s_IsVolkInitialized)
    {
        volkInitialize();
        s_IsVolkInitialized = true;
    }

    m_Extensions.FillExtensionsContainer();
}

void luvk::Renderer::RegisterModules(std::vector<std::shared_ptr<IRenderModule>> Modules)
{
    m_RenderModules = std::move(Modules);
}

bool luvk::Renderer::InitializeRenderer(InstanceCreationArguments const& Arguments, void const* pNext)
{
    if (!s_IsVolkInitialized)
    {
        throw std::runtime_error("Volk is not initialized, ensure that you call PreInitializeRenderer() before executing this function.");
    }

    void const* FeatureChain = pNext;
    for (auto const& Module : m_RenderModules)
    {
        for (auto const& Ext : Module->GetRequiredInstanceExtensions())
        {
            m_Extensions.SetLayerState(Ext.first, true);

            for (std::string_view const ExtensionIt : Ext.second)
            {
                m_Extensions.SetExtensionState(Ext.first, ExtensionIt, true);
            }
        }

        if (auto const Chain = Module->GetInstanceFeatureChain(shared_from_this()))
        {
            auto* Base = const_cast<VkBaseInStructure*>(static_cast<VkBaseInStructure const*>(Chain));
            Base->pNext = static_cast<VkBaseInStructure const*>(FeatureChain);
            FeatureChain = Chain;
        }
    }

    VkApplicationInfo const AppInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName = std::data(Arguments.ApplicationName),
                                    .applicationVersion = Arguments.ApplicationVersion,
                                    .pEngineName = std::data(Arguments.EngineName),
                                    .engineVersion = Arguments.EngineVersion,
                                    .apiVersion = VK_API_VERSION_1_4};

    auto const Layers = m_Extensions.GetEnabledLayersNames();
    auto const Extensions = m_Extensions.GetEnabledExtensionsNames();

    VkInstanceCreateInfo const InstanceCreateInfo{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
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
        return true;
    }

    return false;
}

void luvk::Renderer::PostInitializeRenderer()
{
    InitializeDependencies(nullptr);
    GetEventSystem().Execute(RendererEvents::OnPostInitialized);
}

void luvk::Renderer::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    std::for_each(std::execution::seq,
                  std::begin(m_RenderModules),
                  std::end(m_RenderModules),
                  [this](const auto& ModuleIt)
                  {
                      ModuleIt->InitializeDependencies(shared_from_this());
                  });
}

void luvk::Renderer::ClearResources()
{
    const auto DeviceModule = FindModule<luvk::Device>();
    const VkDevice LogicalDevice = DeviceModule
                                       ? DeviceModule->GetLogicalDevice()
                                       : VK_NULL_HANDLE;

    if (LogicalDevice != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(LogicalDevice);
    }

    for (auto& Destructor : m_ExternalDestructors)
    {
        Destructor();
    }
    m_ExternalDestructors.clear();

    std::for_each(std::execution::seq,
                  std::rbegin(m_RenderModules),
                  std::rend(m_RenderModules),
                  [this](std::shared_ptr<IRenderModule> const& ModuleIt)
                  {
                      ModuleIt->ClearResources();
                  });

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }

    if (s_IsVolkInitialized)
    {
        volkFinalize();
        s_IsVolkInitialized = false;
    }
}

void luvk::Renderer::InitializeRenderLoop(std::shared_ptr<Device> const& Device,
                                          std::shared_ptr<SwapChain> const& Swap,
                                          std::shared_ptr<CommandPool> const& Pool,
                                          std::shared_ptr<MeshRegistry> const& Registry,
                                          std::shared_ptr<ThreadPool> const& ThreadPoolMod)
{
    m_DeviceModule = Device;
    m_SwapChainModule = Swap;
    m_CommandPoolModule = Pool;
    m_MeshRegistryModule = Registry;
    m_ThreadPoolModule = ThreadPoolMod;

    SetupFrames();
    GetEventSystem().Execute(RendererEvents::OnRenderLoopInitialized);
}

void luvk::Renderer::DrawFrame()
{
    if (m_Paused)
    {
        return;
    }

    const auto DeviceMod = m_DeviceModule;
    const auto Swap = m_SwapChainModule;
    const VkDevice LogicalDevice = DeviceMod->GetLogicalDevice();

    const auto Sync = FindModule<luvk::Synchronization>();
    auto& Frame = Sync->GetFrame(Sync->GetCurrentFrame());

    if (Frame.Submitted)
    {
        DeviceMod->Wait(Frame.InFlight, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        vkResetFences(LogicalDevice, 1, &Frame.InFlight);
    }

    constexpr std::uint64_t AcquireTimeout = 0;
    std::uint32_t ImageIndex = 0;
    const VkResult AcquireResult = vkAcquireNextImageKHR(LogicalDevice, Swap->GetHandle(), AcquireTimeout, Frame.ImageAvailable, VK_NULL_HANDLE, &ImageIndex);

    if (AcquireResult == VK_NOT_READY || AcquireResult == VK_TIMEOUT || AcquireResult == VK_ERROR_OUT_OF_DATE_KHR || AcquireResult == VK_SUBOPTIMAL_KHR)
    {
        return;
    }

    if (AcquireResult != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to acquire swapchain image.");
    }

    if (!LUVK_EXECUTE(vkResetCommandBuffer(Frame.CommandBuffer, 0)))
    {
        throw std::runtime_error("Failed to reset command buffer.");
    }

    BeginExternalFrame();
    RecordCommands(Frame, ImageIndex);
    SubmitFrame(Frame, ImageIndex);
    EndExternalFrame();
}

void luvk::Renderer::SetPaused(const bool Paused)
{
    m_Paused = Paused;
}

void luvk::Renderer::Refresh(const VkExtent2D Extent)
{
    const auto Swap = m_SwapChainModule;
    const auto DevMod = m_DeviceModule;

    DevMod->WaitIdle();
    Swap->Recreate(m_DeviceModule, m_MemoryModule, Extent, nullptr);
    SetupFrames();
}

void luvk::Renderer::SetRenderTargets(RenderTargets Targets)
{
    if (std::empty(Targets.ColorViews))
    {
        m_CustomTargets.reset();
    }
    else
    {
        m_CustomTargets = std::move(Targets);
    }
}

void luvk::Renderer::EnqueueCommand(std::function<void(VkCommandBuffer)> Cmd)
{
    m_PostRenderCommands.emplace_back(std::move(Cmd));
}

void luvk::Renderer::EnqueueDestructor(std::function<void()> Destructor)
{
    m_ExternalDestructors.emplace_back(std::move(Destructor));
}

VkDescriptorPool luvk::Renderer::CreateExternalDescriptorPool(
    std::uint32_t const MaxSets,
    std::span<VkDescriptorPoolSize const> PoolSizes,
    VkDescriptorPoolCreateFlags const Flags)
{
    auto const Device = m_DeviceModule;
    VkDevice const LogicalDevice = Device->GetLogicalDevice();

    VkDescriptorPool Pool{VK_NULL_HANDLE};
    VkDescriptorPoolCreateInfo const Info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .pNext = nullptr,
                                          .flags = Flags,
                                          .maxSets = MaxSets,
                                          .poolSizeCount = static_cast<std::uint32_t>(std::size(PoolSizes)),
                                          .pPoolSizes = std::data(PoolSizes)};

    if (!LUVK_EXECUTE(vkCreateDescriptorPool(LogicalDevice, &Info, nullptr, &Pool)))
    {
        throw std::runtime_error("Failed to create descriptor pool.");
    }

    EnqueueDestructor([LogicalDevice, Pool]() { vkDestroyDescriptorPool(LogicalDevice, Pool, nullptr); });

    return Pool;
}

void luvk::Renderer::DestroyExternalDescriptorPool(VkDescriptorPool& Pool)
{
    if (Pool == VK_NULL_HANDLE)
    {
        return;
    }

    auto const Device = m_DeviceModule;
    VkDevice const LogicalDevice = Device->GetLogicalDevice();

    vkDestroyDescriptorPool(LogicalDevice, Pool, nullptr);
    Pool = VK_NULL_HANDLE;
}

VkDevice luvk::Renderer::GetLogicalDevice() const
{
    return m_DeviceModule ? m_DeviceModule->GetLogicalDevice() : VK_NULL_HANDLE;
}

VkPhysicalDevice luvk::Renderer::GetPhysicalDevice() const
{
    return m_DeviceModule ? m_DeviceModule->GetPhysicalDevice() : VK_NULL_HANDLE;
}

VkQueue luvk::Renderer::GetGraphicsQueue() const
{
    if (!m_DeviceModule)
    {
        return VK_NULL_HANDLE;
    }

    auto const GraphicsFamily = m_DeviceModule->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value();
    return m_DeviceModule->GetQueue(GraphicsFamily);
}

std::size_t luvk::Renderer::GetSwapChainImageCount() const
{
    return m_SwapChainModule ? std::size(m_SwapChainModule->GetImages()) : 0ULL;
}

VkRenderPass luvk::Renderer::GetRenderPass() const
{
    return m_SwapChainModule ? m_SwapChainModule->GetRenderPass() : VK_NULL_HANDLE;
}

void luvk::Renderer::BeginExternalFrame()
{
    // Intentionally left blank for user hooks
}

void luvk::Renderer::EndExternalFrame()
{
    // Intentionally left blank for user hooks
}

void luvk::Renderer::SetupFrames()
{
    const auto Sync = FindModule<luvk::Synchronization>();
    Sync->SetupFrames(m_DeviceModule, m_SwapChainModule, m_CommandPoolModule, m_ThreadPoolModule);
}

void luvk::Renderer::RecordComputePass(VkCommandBuffer Cmd)
{
    if (!m_MeshRegistryModule)
    {
        return;
    }

    auto const& Meshes = m_MeshRegistryModule->GetMeshes();
    for (auto const& MeshIt : Meshes)
    {
        auto const& Mat = MeshIt.MaterialPtr;
        if (!Mat || !Mat->GetPipeline())
        {
            continue;
        }

        if (Mat->GetPipeline()->GetType() != Pipeline::Type::Compute)
        {
            continue;
        }

        luvk::RecordMeshCommands(Cmd, MeshIt);
    }
}

void luvk::Renderer::RecordGraphicsPass(luvk::Synchronization::FrameData& Frame, std::uint32_t ImageIndex)
{
    const auto Swap = m_SwapChainModule;
    const auto Registry = m_MeshRegistryModule;
    const auto Pool = m_ThreadPoolModule;
    const auto Sync = FindModule<luvk::Synchronization>();

    const VkExtent2D Extent = Swap->GetExtent();

    constexpr std::array Clear{VkClearValue{.color = {0.F, 0.F, 0.F, 1.F}}, VkClearValue{.depthStencil = {1.F, 0}}};
    const VkRenderPassBeginInfo BeginInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                          .renderPass = Swap->GetRenderPass(),
                                          .framebuffer = Swap->GetFramebuffer(ImageIndex),
                                          .renderArea = {{0, 0}, {Extent.width, Extent.height}},
                                          .clearValueCount = static_cast<std::uint32_t>(std::size(Clear)),
                                          .pClearValues = std::data(Clear)};

    bool HasGraphics = false;
    for (auto const& MeshIt : Registry->GetMeshes())
    {
        auto const& Mat = MeshIt.MaterialPtr;
        if (!Mat || !Mat->GetPipeline())
        {
            continue;
        }
        if (Mat->GetPipeline()->GetType() != Pipeline::Type::Compute)
        {
            HasGraphics = true;
            break;
        }
    }

    if (!HasGraphics)
    {
        return;
    }

    vkCmdBeginRenderPass(Frame.CommandBuffer, &BeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    const VkViewport Viewport{0.F, 0.F, static_cast<float>(Extent.width), static_cast<float>(Extent.height), 0.F, 1.F};
    const VkRect2D Scissor{{0, 0}, Extent};

    auto const& Meshes = Registry->GetMeshes();
    constexpr std::size_t BatchSize = 10;
    const std::size_t MeshCount = std::size(Meshes);
    const std::size_t BatchCount = (MeshCount + BatchSize - 1) / BatchSize;

    Frame.SecondaryBuffers.resize(BatchCount);
    auto& SecPool = Sync->GetSecondaryPool();
    SecPool.Reset();

    for (std::size_t BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
    {
        VkCommandBuffer SecCmd = SecPool.Acquire();
        Frame.SecondaryBuffers[BatchIndex] = SecCmd;
        std::size_t StartIndex = BatchIndex * BatchSize;
        std::size_t EndIndex = std::min(MeshCount, StartIndex + BatchSize);

        Pool->Submit([SecCmd, StartIndex, EndIndex, &Viewport, &Scissor, &Meshes, Swap, ImageIndex]()
        {
            VkCommandBufferInheritanceInfo const Inherit{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
                                                         .renderPass = Swap->GetRenderPass(),
                                                         .subpass = 0,
                                                         .framebuffer = Swap->GetFramebuffer(ImageIndex)};

            VkCommandBufferBeginInfo const BeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                     .pNext = nullptr,
                                                     .flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT | VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                                     .pInheritanceInfo = &Inherit};

            vkBeginCommandBuffer(SecCmd, &BeginInfo);

            vkCmdSetViewport(SecCmd, 0, 1, &Viewport);
            vkCmdSetScissor(SecCmd, 0, 1, &Scissor);

            for (std::size_t MeshIndex = StartIndex; MeshIndex < EndIndex; ++MeshIndex)
            {
                auto const& MeshIt = Meshes[MeshIndex];

                auto const& Mat = MeshIt.MaterialPtr;
                if (!Mat || !Mat->GetPipeline())
                {
                    continue;
                }
                if (Mat->GetPipeline()->GetType() == Pipeline::Type::Compute)
                {
                    continue;
                }

                luvk::RecordMeshCommands(SecCmd, MeshIt);
            }

            vkEndCommandBuffer(SecCmd);
        });
    }

    Pool->WaitIdle();

    if (!std::empty(Frame.SecondaryBuffers))
    {
        vkCmdExecuteCommands(Frame.CommandBuffer, static_cast<std::uint32_t>(std::size(Frame.SecondaryBuffers)), std::data(Frame.SecondaryBuffers));
    }

    vkCmdEndRenderPass(Frame.CommandBuffer);
}

void luvk::Renderer::RecordCommands(luvk::Synchronization::FrameData& Frame, std::uint32_t ImageIndex)
{
    constexpr VkCommandBufferBeginInfo Begin{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                             .pNext = nullptr,
                                             .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
                                             .pInheritanceInfo = nullptr};

    if (!LUVK_EXECUTE(vkBeginCommandBuffer(Frame.CommandBuffer, &Begin)))
    {
        throw std::runtime_error("Failed to begin command buffer.");
    }

    RecordComputePass(Frame.CommandBuffer);
    RecordGraphicsPass(Frame, ImageIndex);

    for (auto& Cmd : m_PostRenderCommands)
    {
        Cmd(Frame.CommandBuffer);
    }
    m_PostRenderCommands.clear();
}

void luvk::Renderer::SubmitFrame(luvk::Synchronization::FrameData& Frame, const std::uint32_t ImageIndex)
{
    const auto DevMod = m_DeviceModule;
    const auto Swap = m_SwapChainModule;
    const auto Sync = FindModule<luvk::Synchronization>();

    if (!LUVK_EXECUTE(vkEndCommandBuffer(Frame.CommandBuffer)))
    {
        throw std::runtime_error("Failed to record command buffer.");
    }

    std::array const WaitSemaphores{Frame.ImageAvailable};
    std::array<VkPipelineStageFlags, 1> const WaitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    const VkSubmitInfo Submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext = nullptr,
                              .waitSemaphoreCount = 1,
                              .pWaitSemaphores = std::data(WaitSemaphores),
                              .pWaitDstStageMask = std::data(WaitStages),
                              .commandBufferCount = 1,
                              .pCommandBuffers = &Frame.CommandBuffer,
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores = &Sync->GetRenderFinished(ImageIndex)};

    const std::uint32_t GraphicsFamily = DevMod->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value();
    const VkQueue GraphicsQueue = DevMod->GetQueue(GraphicsFamily);

    if (!LUVK_EXECUTE(vkQueueSubmit(GraphicsQueue, 1, &Submit, VK_NULL_HANDLE)))
    {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }

    // Frame.Submitted = true;

    const VkSwapchainKHR Handle = Swap->GetHandle();
    VkPresentInfoKHR const Present{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                   .pNext = nullptr,
                                   .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = &Sync->GetRenderFinished(ImageIndex),
                                   .swapchainCount = 1,
                                   .pSwapchains = &Handle,
                                   .pImageIndices = &ImageIndex,
                                   .pResults = nullptr};

    const VkResult PresentResult = vkQueuePresentKHR(GraphicsQueue, &Present);
    if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR || PresentResult == VK_SUBOPTIMAL_KHR)
    {
        return;
    }

    if (PresentResult != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swapchain image.");
    }

    Sync->AdvanceFrame();
}
