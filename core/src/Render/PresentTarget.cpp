// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Render/PresentTarget.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Vulkan.hpp"

luvk::PresentTarget::PresentTarget(Context& Ctx, const PresentTargetDesc& Desc)
    : m_Context(Ctx),
      m_Surface(Desc.Surface),
      m_OwnsSurface(Desc.OwnsSurface),
      m_Name(Desc.Name),
      m_PendingExtent(Desc.Swapchain.Extent)
{
    if (m_Surface == VK_NULL_HANDLE)
    {
        throw std::invalid_argument("[luvk] A PresentTarget requires a surface.");
    }

    const std::uint32_t FrameCount = std::max(1U, Desc.FramesInFlight);
    m_States.resize(FrameCount);
    m_AcquiredImages.resize(FrameCount, 0U);

    try
    {
        constexpr VkSemaphoreCreateInfo SemaphoreInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        m_AcquireSemaphores.resize(FrameCount, VK_NULL_HANDLE);
        for (std::uint32_t Index = 0U; Index < FrameCount; ++Index)
        {
            Check(vkCreateSemaphore(Ctx.Device(), &SemaphoreInfo, nullptr, &m_AcquireSemaphores[Index]), "vkCreateSemaphore (acquire)");
            Ctx.SetDebugName(m_AcquireSemaphores[Index], VK_OBJECT_TYPE_SEMAPHORE, m_Name + " acquire " + std::to_string(Index));
        }

        SwapchainDesc Swapchain = Desc.Swapchain;
        Swapchain.Surface       = m_Surface;
        m_Swapchain.emplace(Ctx, Swapchain);
    }
    catch (...)
    {
        DestroySemaphores();
        if (m_OwnsSurface)
        {
            vkDestroySurfaceKHR(Ctx.Instance(), m_Surface, nullptr);
        }
        throw;
    }

    m_NeedsRecreate = !m_Swapchain->IsValid();
}

luvk::PresentTarget::~PresentTarget()
{
    // The swapchain destructor waits for the graphics queue and every pending present before releasing its images.
    m_Swapchain.reset();
    DestroySemaphores();

    if (m_OwnsSurface && m_Surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_Context.Instance(), m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }
}

void luvk::PresentTarget::DestroySemaphores() noexcept
{
    for (VkSemaphore& Semaphore : m_AcquireSemaphores)
    {
        if (Semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(m_Context.Device(), Semaphore, nullptr);
            Semaphore = VK_NULL_HANDLE;
        }
    }
}

void luvk::PresentTarget::Resize(const Extent2D NewExtent) noexcept
{
    m_PendingExtent = NewExtent;
    m_NeedsRecreate = true;
}

luvk::Extent2D luvk::PresentTarget::Extent() const noexcept
{
    return IsRenderable() ? m_Swapchain->Extent() : Extent2D{};
}

luvk::Format luvk::PresentTarget::Format() const noexcept
{
    return IsRenderable() ? m_Swapchain->Format() : Format::Undefined;
}

VkSwapchainKHR luvk::PresentTarget::SwapchainHandle() const noexcept
{
    return m_Swapchain.has_value() ? m_Swapchain->Handle() : VK_NULL_HANDLE;
}

void luvk::PresentTarget::SetProvider(std::shared_ptr<SwapchainProvider> Provider)
{
    if (!m_Swapchain.has_value())
    {
        throw std::logic_error("[luvk] PresentTarget::SetProvider: the target has no swapchain.");
    }

    // The images of every frame slot are either presented or never acquired here: their states restart at UNDEFINED.
    const bool Created = m_Swapchain->SetProvider(std::move(Provider));
    m_NeedsRecreate    = !Created;
    for (ResourceState& State : m_States)
    {
        State = ResourceState{};
    }
}

bool luvk::PresentTarget::SetVSync(const bool Enabled)
{
    if (!m_Swapchain.has_value())
    {
        return false;
    }

    const bool Created = m_Swapchain->SetVSync(Enabled);
    m_NeedsRecreate    = !Created;
    for (ResourceState& State : m_States)
    {
        State = ResourceState{};
    }
    return Created;
}

std::shared_ptr<luvk::SwapchainProvider> luvk::PresentTarget::Provider() const noexcept
{
    return m_Swapchain.has_value() ? m_Swapchain->Provider() : nullptr;
}

luvk::CompositeAlpha luvk::PresentTarget::CompositeAlpha() const noexcept
{
    return m_Swapchain.has_value() ? m_Swapchain->CompositeAlpha() : CompositeAlpha::Opaque;
}

luvk::PresentMode luvk::PresentTarget::PresentMode() const noexcept
{
    return m_Swapchain.has_value() ? m_Swapchain->PresentMode() : PresentMode::Fifo;
}

bool luvk::PresentTarget::EnsureSwapchain()
{
    if (!m_Swapchain.has_value())
    {
        return false;
    }

    if (m_NeedsRecreate || !m_Swapchain->IsValid())
    {
        if (!m_Swapchain->Recreate(m_PendingExtent))
        {
            return false;
        }
        m_NeedsRecreate = false;
    }

    return m_Swapchain->IsValid();
}

std::optional<luvk::PresentImage> luvk::PresentTarget::Acquire(RenderGraph& Graph, const std::uint32_t FrameSlot)
{
    if (!EnsureSwapchain())
    {
        return std::nullopt;
    }

    std::uint32_t ImageIndex = 0U;
    switch (m_Swapchain->Acquire(m_AcquireSemaphores.at(FrameSlot), ImageIndex))
    {
    case SwapchainStatus::Ok:
        break;
    case SwapchainStatus::Suboptimal:
        m_NeedsRecreate = true;
        break;
    case SwapchainStatus::OutOfDate:
        m_NeedsRecreate = true;
        return std::nullopt;
    }

    m_AcquiredImages[FrameSlot] = ImageIndex;
    return PresentImage{.Texture    = Reimport(Graph, FrameSlot),
                        .Extent     = m_Swapchain->Extent(),
                        .Format     = m_Swapchain->Format(),
                        .ImageIndex = ImageIndex};
}

luvk::TextureHandle luvk::PresentTarget::Reimport(RenderGraph& Graph, const std::uint32_t FrameSlot)
{
    const std::uint32_t ImageIndex = m_AcquiredImages.at(FrameSlot);
    const Extent2D      Extent     = m_Swapchain->Extent();

    // The presentation engine owns the image between frames: its previous content and layout are undefined.
    ResourceState& State = m_States.at(FrameSlot);
    State                = ResourceState{};

    return Graph.ImportExternal({.Image            = m_Swapchain->Image(ImageIndex),
                                 .View             = m_Swapchain->View(ImageIndex),
                                 .Format           = m_Swapchain->Format(),
                                 .Extent           = {Extent.Width, Extent.Height, 1U},
                                 .State            = &State,
                                 .WaitsOnSemaphore = true,
                                 .Name             = m_Name});
}

VkSemaphoreSubmitInfo luvk::PresentTarget::AcquireWait(const RenderGraph& Graph, const TextureHandle Texture, const std::uint32_t FrameSlot) const
{
    return VkSemaphoreSubmitInfo{.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                 .semaphore = m_AcquireSemaphores.at(FrameSlot),
                                 .stageMask = Graph.FirstUseStage(Texture)};
}

VkSemaphoreSubmitInfo luvk::PresentTarget::RenderFinishedSignal(const std::uint32_t FrameSlot) const
{
    return VkSemaphoreSubmitInfo{.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                 .semaphore = m_Swapchain->RenderFinished(m_AcquiredImages.at(FrameSlot)),
                                 .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT};
}

void luvk::PresentTarget::WaitForPresent(const std::uint32_t FrameSlot)
{
    m_Swapchain->WaitForPresent(m_AcquiredImages.at(FrameSlot));
}

void luvk::PresentTarget::Present(const std::uint32_t FrameSlot)
{
    if (m_Swapchain->Present(m_AcquiredImages.at(FrameSlot)) != SwapchainStatus::Ok)
    {
        m_NeedsRecreate = true;
    }
}
