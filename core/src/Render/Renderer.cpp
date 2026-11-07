// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Render/Renderer.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <string>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Upload.hpp"

namespace
{
    VkCommandPool CreatePool(luvk::Context& Ctx)
    {
        const VkCommandPoolCreateInfo Info{.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                           .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                           .queueFamilyIndex = Ctx.GraphicsQueue().Family()};
        VkCommandPool Pool = VK_NULL_HANDLE;
        luvk::Check(vkCreateCommandPool(Ctx.Device(), &Info, nullptr, &Pool), "vkCreateCommandPool");
        return Pool;
    }

    VkCommandBuffer AllocateCommands(const VkDevice Device, const VkCommandPool Pool)
    {
        const VkCommandBufferAllocateInfo Info{.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                               .commandPool        = Pool,
                                               .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                               .commandBufferCount = 1U};
        VkCommandBuffer Commands = VK_NULL_HANDLE;
        luvk::Check(vkAllocateCommandBuffers(Device, &Info, &Commands), "vkAllocateCommandBuffers");
        return Commands;
    }

    /** Main window target: the explicit surface, else the Context's window surface, else none (headless). */
    std::unique_ptr<luvk::PresentTarget> CreateMainTarget(luvk::Context& Ctx, const luvk::RendererDesc& Desc)
    {
        const bool         Explicit = Desc.Swapchain.Surface != VK_NULL_HANDLE;
        const VkSurfaceKHR Surface  = Explicit ? Desc.Swapchain.Surface : Ctx.ReleaseSurface();
        if (Surface == VK_NULL_HANDLE)
        {
            return nullptr;
        }

        return std::make_unique<luvk::PresentTarget>(Ctx,
                                                     luvk::PresentTargetDesc{.Surface        = Surface,
                                                                             .OwnsSurface    = !Explicit,
                                                                             .Swapchain      = Desc.Swapchain,
                                                                             .FramesInFlight = std::max(1U, Desc.FramesInFlight),
                                                                             .Name           = "backbuffer"});
    }
}

luvk::Renderer::Renderer(Context& Ctx, const RendererDesc& Desc)
    : m_Context(Ctx),
      m_MainTarget(CreateMainTarget(Ctx, Desc)),
      m_Allocator(Ctx, std::max(1U, Desc.FramesInFlight))
{
    const std::uint32_t Count = std::max(1U, Desc.FramesInFlight);
    m_Slots.resize(Count);

    try
    {
        for (std::uint32_t Index = 0U; Index < Count; ++Index)
        {
            FrameSlot& Slot = m_Slots[Index];
            Slot.Pool       = CreatePool(Ctx);
            Slot.Commands   = AllocateCommands(Ctx.Device(), Slot.Pool);
            Slot.Graph      = std::make_unique<RenderGraph>(Ctx);
            Ctx.SetDebugName(Slot.Commands, VK_OBJECT_TYPE_COMMAND_BUFFER, "luvk frame " + std::to_string(Index));
        }
    }
    catch (...)
    {
        DestroySlots();
        throw;
    }

    if (Desc.GpuTimings)
    {
        m_Profiler = std::make_unique<GpuProfiler>(Ctx, Count);
    }
}

luvk::Renderer::~Renderer()
{
    for (const FrameSlot& Slot : m_Slots)
    {
        try
        {
            m_Context.GraphicsQueue().Wait(Slot.Value);
        }
        catch (...)
        {
        }
    }

    DestroySlots();
    m_MainTarget.reset();
}

void luvk::Renderer::DestroySlots() noexcept
{
    for (FrameSlot& Slot : m_Slots)
    {
        Slot.Graph.reset();
        if (Slot.Pool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(m_Context.Device(), Slot.Pool, nullptr);
            Slot.Pool = VK_NULL_HANDLE;
        }
    }
}

std::optional<luvk::Frame> luvk::Renderer::BeginFrame()
{
    if (m_FrameActive)
    {
        throw std::logic_error("[luvk] BeginFrame called twice without EndFrame / AbortFrame.");
    }

    const std::uint32_t SlotIndex = static_cast<std::uint32_t>(m_FrameNumber % std::size(m_Slots));
    FrameSlot&          Slot      = m_Slots[SlotIndex];

    // Waiting for the slot also makes every acquire semaphore of this slot reusable.
    m_Context.GraphicsQueue().Wait(Slot.Value);
    m_Context.CollectGarbage();
    m_Allocator.BeginFrame(SlotIndex);

    RenderGraph& Graph = *Slot.Graph;
    Graph.Reset();

    // Headless renderers record frames without any window: the frame simply has no backbuffer.
    std::optional<PresentImage> Main;
    if (m_MainTarget)
    {
        Main = m_MainTarget->Acquire(Graph, SlotIndex);
        if (!Main)
        {
            return std::nullopt;
        }
    }

    Check(vkResetCommandPool(m_Context.Device(), Slot.Pool, 0U), "vkResetCommandPool");

    m_Acquired.clear();
    if (Main)
    {
        m_Acquired.emplace_back(m_MainTarget.get(), Main->Texture);
    }
    m_Context.BeginFrameRetirement();
    m_FrameActive = true;

    return Frame{.Graph      = Graph,
                 .Allocator  = m_Allocator,
                 .Backbuffer = Main ? Main->Texture : TextureHandle{},
                 .Extent     = Main ? Main->Extent : VkExtent2D{},
                 .Format     = Main ? Main->Format : VK_FORMAT_UNDEFINED,
                 .FrameSlot  = SlotIndex,
                 .Number     = m_FrameNumber};
}

std::optional<luvk::PresentImage> luvk::Renderer::AcquireTarget(PresentTarget& Target)
{
    if (!m_FrameActive)
    {
        throw std::logic_error("[luvk] AcquireTarget must be called between BeginFrame and EndFrame.");
    }

    if (std::ranges::any_of(m_Acquired, [&](const auto& Entry) { return Entry.first == &Target; }))
    {
        throw std::logic_error("[luvk] A present target can only be acquired once per frame.");
    }

    const std::uint32_t                SlotIndex = static_cast<std::uint32_t>(m_FrameNumber % std::size(m_Slots));
    std::optional<PresentImage>        Image     = Target.Acquire(*m_Slots[SlotIndex].Graph, SlotIndex);
    if (Image)
    {
        m_Acquired.emplace_back(&Target, Image->Texture);
    }
    return Image;
}

void luvk::Renderer::EndFrame(const Frame& Current)
{
    if (!m_FrameActive)
    {
        throw std::logic_error("[luvk] EndFrame called without a matching BeginFrame.");
    }

    try
    {
        SubmitAndPresent(Current, true);
    }
    catch (...)
    {
        // Recording or submission failed: still balance the acquisitions so the next frame can start. Once the
        // frame was submitted (a present failed) there is nothing left to close.
        if (m_FrameActive)
        {
            CloseFrame(Current);
        }
        throw;
    }
}

void luvk::Renderer::AbortFrame(const Frame& Current)
{
    if (!m_FrameActive)
    {
        throw std::logic_error("[luvk] AbortFrame called without a matching BeginFrame.");
    }
    CloseFrame(Current);
}

void luvk::Renderer::CloseFrame(const Frame& Current) noexcept
{
    try
    {
        Check(vkResetCommandPool(m_Context.Device(), m_Slots.at(Current.FrameSlot).Pool, 0U), "vkResetCommandPool");

        RenderGraph& Graph = *m_Slots[Current.FrameSlot].Graph;
        Graph.Reset();
        for (auto& [Target, Texture] : m_Acquired)
        {
            Texture = Target->Reimport(Graph, Current.FrameSlot);
        }

        SubmitAndPresent(Current, false);
    }
    catch (...)
    {
        // Nothing else can be done for this frame; at least leave the renderer in a state that can begin again.
        m_Context.SealFrameRetirement(m_Context.GraphicsQueue().NextValue());
        for (const auto& Entry : m_Acquired)
        {
            Entry.first->m_NeedsRecreate = true;
        }
        m_Acquired.clear();
        m_FrameActive = false;
        ++m_FrameNumber;
    }
}

void luvk::Renderer::SubmitAndPresent(const Frame& Current, const bool Profile)
{
    FrameSlot&   Slot  = m_Slots.at(Current.FrameSlot);
    RenderGraph& Graph = *Slot.Graph;

    for (const auto& [Target, Texture] : m_Acquired)
    {
        Graph.SetFinalAccess(Texture, Access::Present);
    }

    constexpr VkCommandBufferBeginInfo BeginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                                                 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT};
    Check(vkBeginCommandBuffer(Slot.Commands, &BeginInfo), "vkBeginCommandBuffer");

    GpuProfiler* const Profiler = Profile ? m_Profiler.get() : nullptr;
    if (Profiler != nullptr)
    {
        Profiler->BeginFrame(Slot.Commands, Current.FrameSlot);
    }
    Graph.SetProfiler(Profiler);
    Graph.Execute(Slot.Commands);
    if (Profiler != nullptr)
    {
        Profiler->EndFrame(Slot.Commands);
    }
    Check(vkEndCommandBuffer(Slot.Commands), "vkEndCommandBuffer");
    m_Allocator.EndFrame();

    std::vector<VkSemaphoreSubmitInfo> Waits;
    std::vector<VkSemaphoreSubmitInfo> Signals;
    Waits.reserve(std::size(m_Acquired) + 1U);
    Signals.reserve(std::size(m_Acquired));

    for (const auto& [Target, Texture] : m_Acquired)
    {
        Waits.push_back(Target->AcquireWait(Graph, Texture, Current.FrameSlot));
        Signals.push_back(Target->RenderFinishedSignal(Current.FrameSlot));
        Target->WaitForPresent(Current.FrameSlot);
    }

    if (const std::uint64_t UploadValue = m_Context.Uploads().Flush(); UploadValue != 0U)
    {
        Waits.push_back(VkSemaphoreSubmitInfo{.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                              .semaphore = m_Context.TransferQueue().Timeline(),
                                              .value     = UploadValue,
                                              .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT});
    }

    const VkCommandBufferSubmitInfo CommandInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = Slot.Commands};
    Slot.Value = m_Context.GraphicsQueue().Submit({&CommandInfo, 1U}, Waits, Signals);

    // Everything retired while this frame was recorded may still be referenced by it: release it with this value.
    m_Context.SealFrameRetirement(Slot.Value);
    m_FrameActive = false;
    ++m_FrameNumber;

    // Present every target even if one fails, then report the first failure.
    std::exception_ptr Failure;
    for (const auto& [Target, Texture] : m_Acquired)
    {
        try
        {
            Target->Present(Current.FrameSlot);
        }
        catch (...)
        {
            Target->m_NeedsRecreate = true;
            if (!Failure)
            {
                Failure = std::current_exception();
            }
        }
    }
    m_Acquired.clear();

    if (Failure)
    {
        std::rethrow_exception(Failure);
    }
}

void luvk::Renderer::Resize(const VkExtent2D NewExtent) noexcept
{
    if (m_MainTarget)
    {
        m_MainTarget->Resize(NewExtent);
    }
}
