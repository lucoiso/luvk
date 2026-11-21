// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "ContextInternals.hpp"
#include <exception>
#include <mutex>
#include <string>
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Error.hpp"

luvk::TimelinePoint luvk::Context::CurrentRetirePoint() const noexcept
{
    std::scoped_lock Lock(m_FrameRetireMutex);
    return TimelinePoint{.Graphics = m_FrameRecording ? PendingFrameValue : m_GraphicsQueue.NextValue(), .Transfer = 0U};
}

void luvk::Context::Retire(RetiredObject&& Object) noexcept
{
    Retire(std::move(Object), CurrentRetirePoint());
}

void luvk::Context::Retire(RetiredObject&& Object, const TimelinePoint& Point) noexcept
{
    try
    {
        if (Point.Graphics != PendingFrameValue)
        {
            m_DeletionQueue.Push(std::move(Object), Point);
            return;
        }

        std::scoped_lock Lock(m_FrameRetireMutex);
        if (m_FrameRecording)
        {
            m_FrameRetired.emplace_back(std::move(Object), Point.Transfer);
            return;
        }

        // The frame was sealed between CurrentRetirePoint() and this call: any later submit covers it.
        m_DeletionQueue.Push(std::move(Object), TimelinePoint{.Graphics = m_GraphicsQueue.NextValue(), .Transfer = Point.Transfer});
    }
    catch (...)
    {
        // Out of memory while queueing: the only safe destruction left is after the device went idle. Resource
        // destructors are noexcept and reach this path, so it must not throw.
        if (m_Device != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(m_Device);
        }
        DestroyRetired(Object);
    }
}

void luvk::Context::BeginFrameRetirement() noexcept
{
    std::scoped_lock Lock(m_FrameRetireMutex);
    m_FrameRecording = true;
}

void luvk::Context::SealFrameRetirement(const std::uint64_t GraphicsValue)
{
    std::scoped_lock Lock(m_FrameRetireMutex);
    m_FrameRecording = false;
    for (auto& [Object, Transfer] : m_FrameRetired)
    {
        m_DeletionQueue.Push(std::move(Object), TimelinePoint{.Graphics = GraphicsValue, .Transfer = Transfer});
    }
    m_FrameRetired.clear();
}

void luvk::Context::RetireCallback(std::function<void()> Callback)
{
    Retire(RetiredObject{.Kind = RetiredKind::Callback, .Callback = std::move(Callback)});
}

std::size_t luvk::Context::CollectGarbage()
{
    const TimelinePoint Completed{.Graphics = m_GraphicsQueue.CompletedValue(), .Transfer = m_TransferQueue.CompletedValue()};

    return m_DeletionQueue.Collect(Completed,
                                   [this](RetiredObject& Object)
                                   {
                                       DestroyRetired(Object);
                                   });
}
void luvk::Context::DestroyRetired(RetiredObject& Object) noexcept
{
    switch (Object.Kind)
    {
    case RetiredKind::Buffer:
        vmaDestroyBuffer(m_Allocator, HandleFromU64<VkBuffer>(Object.Handle), Object.Allocation);
        break;
    case RetiredKind::Image:
        vmaDestroyImage(m_Allocator, HandleFromU64<VkImage>(Object.Handle), Object.Allocation);
        break;
    case RetiredKind::ImageView:
        vkDestroyImageView(m_Device, HandleFromU64<VkImageView>(Object.Handle), nullptr);
        break;
    case RetiredKind::Sampler:
        vkDestroySampler(m_Device, HandleFromU64<VkSampler>(Object.Handle), nullptr);
        break;
    case RetiredKind::Pipeline:
        vkDestroyPipeline(m_Device, HandleFromU64<VkPipeline>(Object.Handle), nullptr);
        break;
    case RetiredKind::CommandPool:
        vkDestroyCommandPool(m_Device, HandleFromU64<VkCommandPool>(Object.Handle), nullptr);
        break;
    case RetiredKind::AccelerationStructure:
        vkDestroyAccelerationStructureKHR(m_Device, HandleFromU64<VkAccelerationStructureKHR>(Object.Handle), nullptr);
        break;
    case RetiredKind::BindlessSlot:
        if (m_Bindless)
        {
            m_Bindless->Free(static_cast<BindlessSlot>(Object.Slot), Object.Index);
        }
        break;
    case RetiredKind::Callback:
        if (Object.Callback)
        {
            try
            {
                Object.Callback();
            }
            catch (const std::exception& Exception)
            {
                EmitMessage(DebugSeverity::Error, std::string("[luvk] Retired callback threw: ") + Exception.what());
            }
        }
        break;
    }
}
