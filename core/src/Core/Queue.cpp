// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Queue.hpp"
#include <vector>
#include "luvk/Core/Error.hpp"

luvk::Queue::~Queue()
{
    Destroy();
}

void luvk::Queue::Initialize(const VkDevice Device, const VkQueue Handle, const std::uint32_t Family, std::mutex& Mutex)
{
    m_Device = Device;
    m_Queue  = Handle;
    m_Family = Family;
    m_Mutex  = &Mutex;
    m_NextValue.store(1U, std::memory_order_release);

    constexpr VkSemaphoreTypeCreateInfo TypeInfo{.sType         = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
                                                 .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
                                                 .initialValue  = 0U};

    const VkSemaphoreCreateInfo CreateInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext = &TypeInfo};
    Check(vkCreateSemaphore(m_Device, &CreateInfo, nullptr, &m_Timeline), "vkCreateSemaphore (timeline)");
}

void luvk::Queue::Destroy() noexcept
{
    if (m_Timeline != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_Device, m_Timeline, nullptr);
        m_Timeline = VK_NULL_HANDLE;
    }
    m_Queue = VK_NULL_HANDLE;
}

std::uint64_t luvk::Queue::Submit(const std::span<const VkCommandBufferSubmitInfo> CommandBuffers,
                                  const std::span<const VkSemaphoreSubmitInfo>     Waits,
                                  const std::span<const VkSemaphoreSubmitInfo>     Signals)
{
    std::scoped_lock Lock(*m_Mutex);

    const std::uint64_t Value = m_NextValue.load(std::memory_order_relaxed);

    std::vector<VkSemaphoreSubmitInfo> SignalInfos;
    SignalInfos.reserve(std::size(Signals) + 1U);
    SignalInfos.assign(std::begin(Signals), std::end(Signals));
    SignalInfos.push_back(VkSemaphoreSubmitInfo{.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                                                .semaphore = m_Timeline,
                                                .value     = Value,
                                                .stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT});

    const VkSubmitInfo2 SubmitInfo{.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                                   .waitSemaphoreInfoCount   = static_cast<std::uint32_t>(std::size(Waits)),
                                   .pWaitSemaphoreInfos      = std::data(Waits),
                                   .commandBufferInfoCount   = static_cast<std::uint32_t>(std::size(CommandBuffers)),
                                   .pCommandBufferInfos      = std::data(CommandBuffers),
                                   .signalSemaphoreInfoCount = static_cast<std::uint32_t>(std::size(SignalInfos)),
                                   .pSignalSemaphoreInfos    = std::data(SignalInfos)};

    Check(vkQueueSubmit2(m_Queue, 1U, &SubmitInfo, VK_NULL_HANDLE), "vkQueueSubmit2");

    m_NextValue.store(Value + 1U, std::memory_order_release);
    return Value;
}

VkResult luvk::Queue::Present(const VkPresentInfoKHR& Info)
{
    std::scoped_lock Lock(*m_Mutex);
    return vkQueuePresentKHR(m_Queue, &Info);
}

void luvk::Queue::WaitIdle()
{
    std::scoped_lock Lock(*m_Mutex);
    Check(vkQueueWaitIdle(m_Queue), "vkQueueWaitIdle");
}

bool luvk::Queue::Wait(const std::uint64_t Value, const std::uint64_t TimeoutNs) const
{
    if (Value == 0U)
    {
        return true;
    }

    const VkSemaphoreWaitInfo WaitInfo{.sType          = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO,
                                       .semaphoreCount = 1U,
                                       .pSemaphores    = &m_Timeline,
                                       .pValues        = &Value};

    const VkResult Result = vkWaitSemaphores(m_Device, &WaitInfo, TimeoutNs);
    Check(Result, "vkWaitSemaphores");
    return Result == VK_SUCCESS;
}

std::uint64_t luvk::Queue::CompletedValue() const
{
    std::uint64_t Value = 0U;
    Check(vkGetSemaphoreCounterValue(m_Device, m_Timeline, &Value), "vkGetSemaphoreCounterValue");
    return Value;
}
