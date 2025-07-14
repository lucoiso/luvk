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

void luvk::Renderer::RegisterModules(std::vector<std::shared_ptr<IRenderModule>>&& Modules)
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

void luvk::Renderer::InitializeRenderLoop()
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

    const auto DeviceModule = FindModule<luvk::Device>();
    const auto SwapChainModule = FindModule<luvk::SwapChain>();

    const VkDevice LogicalDevice = DeviceModule->GetLogicalDevice();

    const auto Sync = FindModule<luvk::Synchronization>();
    auto& Frame = Sync->GetFrame(Sync->GetCurrentFrame());

    if (Frame.Submitted)
    {
        DeviceModule->Wait(Frame.InFlight, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        vkResetFences(LogicalDevice, 1, &Frame.InFlight);
    }

    constexpr std::uint64_t AcquireTimeout = 0;
    std::uint32_t ImageIndex = 0;
    const VkResult AcquireResult = vkAcquireNextImageKHR(LogicalDevice,
                                                         SwapChainModule->GetHandle(),
                                                         AcquireTimeout,
                                                         Frame.ImageAvailable,
                                                         VK_NULL_HANDLE,
                                                         &ImageIndex);

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

void luvk::Renderer::Refresh(const VkExtent2D& Extent) const
{
    const auto DeviceModule = FindModule<luvk::Device>();
    const auto SwapChainModule = FindModule<luvk::SwapChain>();
    const auto MemoryModule = FindModule<luvk::Memory>();

    DeviceModule->WaitIdle();
    SwapChainModule->Recreate(DeviceModule, MemoryModule, Extent, nullptr);

    SetupFrames();
}

void luvk::Renderer::EnqueueCommand(std::function<void(VkCommandBuffer)>&& Cmd)
{
    m_PostRenderCommands.emplace_back(std::move(Cmd));
}

void luvk::Renderer::EnqueueDestructor(std::function<void()>&& Destructor)
{
    m_ExternalDestructors.emplace_back(std::move(Destructor));
}

VkDescriptorPool luvk::Renderer::CreateExternalDescriptorPool(std::uint32_t const MaxSets,
                                                              std::span<VkDescriptorPoolSize const> PoolSizes,
                                                              VkDescriptorPoolCreateFlags const Flags)
{
    const auto DeviceModule = FindModule<luvk::Device>();
    VkDevice const LogicalDevice = DeviceModule->GetLogicalDevice();

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

    EnqueueDestructor([LogicalDevice, Pool]()
    {
        vkDestroyDescriptorPool(LogicalDevice, Pool, nullptr);
    });

    return Pool;
}

void luvk::Renderer::DestroyExternalDescriptorPool(VkDescriptorPool& Pool) const
{
    if (Pool == VK_NULL_HANDLE)
    {
        return;
    }

    const auto DeviceModule = FindModule<luvk::Device>();
    VkDevice const LogicalDevice = DeviceModule->GetLogicalDevice();

    vkDestroyDescriptorPool(LogicalDevice, Pool, nullptr);
    Pool = VK_NULL_HANDLE;
}

void luvk::Renderer::BeginExternalFrame() {}

void luvk::Renderer::EndExternalFrame() {}

void luvk::Renderer::SetupFrames() const
{
    const auto SyncModule = FindModule<luvk::Synchronization>();
    const auto DeviceModule = FindModule<luvk::Device>();
    const auto SwapChainModule = FindModule<luvk::SwapChain>();
    const auto CommandPoolModule = FindModule<luvk::CommandPool>();

    SyncModule->SetupFrames(DeviceModule, SwapChainModule, CommandPoolModule);
}

void luvk::Renderer::RecordComputePass(const VkCommandBuffer& Cmd) const
{
    const auto MeshRegistryModule = FindModule<luvk::MeshRegistry>();

    for (auto const& Meshes = MeshRegistryModule->GetMeshes();
         auto const& MeshIt : Meshes)
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

void luvk::Renderer::RecordGraphicsPass(luvk::Synchronization::FrameData& Frame, std::uint32_t ImageIndex) const
{
    const auto SwapChainModule = FindModule<luvk::SwapChain>();
    const auto SyncModule = FindModule<luvk::Synchronization>();
    const auto MeshRegistryModule = FindModule<luvk::MeshRegistry>();
    const auto ThreadPoolModule = FindModule<luvk::ThreadPool>();

    const VkExtent2D Extent = SwapChainModule->GetExtent();

    constexpr std::array Clear{VkClearValue{.color = {0.F, 0.F, 0.F, 1.F}}, VkClearValue{.depthStencil = {1.F, 0}}};
    const VkRenderPassBeginInfo BeginInfo{.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
                                          .renderPass = SwapChainModule->GetRenderPass(),
                                          .framebuffer = SwapChainModule->GetFramebuffer(ImageIndex),
                                          .renderArea = {{0, 0}, {Extent.width, Extent.height}},
                                          .clearValueCount = static_cast<std::uint32_t>(std::size(Clear)),
                                          .pClearValues = std::data(Clear)};

    bool HasGraphics = false;
    for (auto const& MeshIt : MeshRegistryModule->GetMeshes())
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

    VkImage const DepthImage = SwapChainModule->GetDepthImage(ImageIndex)->GetHandle();
    VkImageAspectFlags const DepthAspect = SwapChainModule->GetDepthFormat() == VK_FORMAT_D24_UNORM_S8_UINT ||
                                           SwapChainModule->GetDepthFormat() == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                           SwapChainModule->GetDepthFormat() == VK_FORMAT_D16_UNORM_S8_UINT
                                               ? static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)
                                               : static_cast<VkImageAspectFlags>(VK_IMAGE_ASPECT_DEPTH_BIT);

    VkImageMemoryBarrier2 const DepthBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                             .pNext = nullptr,
                                             .srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                             .srcAccessMask = 0,
                                             .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                             .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                             .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                             .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                                             .image = DepthImage,
                                             .subresourceRange = {DepthAspect, 0, 1, 0, 1}};

    VkDependencyInfo const DepInfo{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                                   .pNext = nullptr,
                                   .dependencyFlags = 0,
                                   .memoryBarrierCount = 0,
                                   .pMemoryBarriers = nullptr,
                                   .bufferMemoryBarrierCount = 0,
                                   .pBufferMemoryBarriers = nullptr,
                                   .imageMemoryBarrierCount = 1,
                                   .pImageMemoryBarriers = &DepthBarrier};

    vkCmdPipelineBarrier2(Frame.CommandBuffer, &DepInfo);

    vkCmdBeginRenderPass(Frame.CommandBuffer, &BeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    const VkViewport Viewport{0.F, 0.F, static_cast<float>(Extent.width), static_cast<float>(Extent.height), 0.F, 1.F};
    const VkRect2D Scissor{{0, 0}, Extent};

    auto const& Meshes = MeshRegistryModule->GetMeshes();
    constexpr std::size_t BatchSize = 10;
    const std::size_t MeshCount = std::size(Meshes);
    const std::size_t BatchCount = (MeshCount + BatchSize - 1) / BatchSize;

    Frame.SecondaryBuffers.resize(BatchCount);
    auto& SecPool = SyncModule->GetSecondaryPool();
    SecPool.Reset();

    for (std::size_t BatchIndex = 0; BatchIndex < BatchCount; ++BatchIndex)
    {
        VkCommandBuffer SecCmd = SecPool.Acquire();
        Frame.SecondaryBuffers[BatchIndex] = SecCmd;
        std::size_t StartIndex = BatchIndex * BatchSize;
        std::size_t EndIndex = std::min(MeshCount, StartIndex + BatchSize);

        ThreadPoolModule->Submit([this, SwapChainModule, SecCmd, StartIndex, EndIndex, &Viewport, &Scissor, &Meshes, ImageIndex]()
        {
            VkCommandBufferInheritanceInfo const Inherit{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO,
                                                         .renderPass = SwapChainModule->GetRenderPass(),
                                                         .subpass = 0,
                                                         .framebuffer = SwapChainModule->GetFramebuffer(ImageIndex)};

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

    ThreadPoolModule->WaitIdle();

    if (!std::empty(Frame.SecondaryBuffers))
    {
        vkCmdExecuteCommands(Frame.CommandBuffer, static_cast<std::uint32_t>(std::size(Frame.SecondaryBuffers)), std::data(Frame.SecondaryBuffers));
    }

    for (auto& Cmd : m_PostRenderCommands)
    {
        Cmd(Frame.CommandBuffer);
    }
    m_PostRenderCommands.clear();

    vkCmdEndRenderPass(Frame.CommandBuffer);
}

void luvk::Renderer::RecordCommands(luvk::Synchronization::FrameData& Frame, const std::uint32_t ImageIndex)
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
}

void luvk::Renderer::SubmitFrame(luvk::Synchronization::FrameData& Frame, const std::uint32_t ImageIndex) const
{
    const auto SyncModule = FindModule<luvk::Synchronization>();
    const auto DeviceModule = FindModule<luvk::Device>();
    const auto SwapChainModule = FindModule<luvk::SwapChain>();

    if (!LUVK_EXECUTE(vkEndCommandBuffer(Frame.CommandBuffer)))
    {
        throw std::runtime_error("Failed to record command buffer.");
    }

    std::array const WaitSemaphores{Frame.ImageAvailable};
    constexpr std::array<VkPipelineStageFlags, 1> WaitStages{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    const VkSubmitInfo Submit{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                              .pNext = nullptr,
                              .waitSemaphoreCount = 1,
                              .pWaitSemaphores = std::data(WaitSemaphores),
                              .pWaitDstStageMask = std::data(WaitStages),
                              .commandBufferCount = 1,
                              .pCommandBuffers = &Frame.CommandBuffer,
                              .signalSemaphoreCount = 1,
                              .pSignalSemaphores = &SyncModule->GetRenderFinished(ImageIndex)};

    const std::uint32_t GraphicsFamily = DeviceModule->FindQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT).value();
    const VkQueue GraphicsQueue = DeviceModule->GetQueue(GraphicsFamily);

    if (!LUVK_EXECUTE(vkQueueSubmit(GraphicsQueue, 1, &Submit, VK_NULL_HANDLE)))
    {
        throw std::runtime_error("Failed to submit draw command buffer.");
    }

    // Frame.Submitted = true;

    const VkSwapchainKHR Handle = SwapChainModule->GetHandle();
    VkPresentInfoKHR const Present{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                                   .pNext = nullptr,
                                   .waitSemaphoreCount = 1,
                                   .pWaitSemaphores = &SyncModule->GetRenderFinished(ImageIndex),
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

    SyncModule->AdvanceFrame();
}
