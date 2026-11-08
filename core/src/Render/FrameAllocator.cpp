// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Render/FrameAllocator.hpp"
#include <bit>
#include <numeric>
#include <string>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace
{

    VkDeviceSize AlignUp(const VkDeviceSize Value, const VkDeviceSize Alignment)
    {
        return (Value + Alignment - 1U) & ~(Alignment - 1U);
    }
}

luvk::FrameAllocator::FrameAllocator(Context& Ctx, const std::uint32_t FramesInFlight, const VkDeviceSize BlockSize)
    : m_Context(Ctx),
      m_BlockSize(std::max<VkDeviceSize>(BlockSize, 64U * 1024U)),
      m_Slots(std::max(1U, FramesInFlight))
{
}

void luvk::FrameAllocator::BeginFrame(const std::uint32_t FrameSlot)
{
    m_Current                 = FrameSlot % static_cast<std::uint32_t>(std::size(m_Slots));
    std::vector<Block>& Slot = m_Slots[m_Current];

    // A slot that overflowed into several blocks is merged into one block of the total size, so steady-state
    // frames allocate from a single buffer. The old blocks are retired (the GPU is done with this slot).
    if (std::size(Slot) > 1U)
    {
        const VkDeviceSize Total = std::accumulate(std::begin(Slot), std::end(Slot), VkDeviceSize{0U},
                                                   [](const VkDeviceSize Sum, const Block& It)
                                                   {
                                                       return Sum + It.Memory.Size();
                                                   });
        Slot.clear();
        Slot.push_back(Block{.Memory = Buffer(m_Context,
                                              {.Size   = std::bit_ceil(Total),

                                               .Memory = MemoryUsage::Stream,
                                               .Name   = "luvk frame allocator " + std::to_string(m_Current)})});
    }

    for (Block& It : Slot)
    {
        It.Used = 0U;
    }
}

void luvk::FrameAllocator::EndFrame() const
{
    for (const Block& It : m_Slots[m_Current])
    {
        if (It.Used > 0U)
        {
            It.Memory.Flush(0U, It.Used);
        }
    }
}

luvk::FrameAllocation luvk::FrameAllocator::Allocate(const VkDeviceSize Size, const VkDeviceSize Alignment)
{
    if (Size == 0U || !std::has_single_bit(Alignment))
    {
        throw std::invalid_argument("[luvk] FrameAllocator::Allocate needs a non-zero size and a power-of-two alignment.");
    }

    std::vector<Block>& Slot = m_Slots[m_Current];

    Block* Target = std::empty(Slot) ? nullptr : &Slot.back();
    if (Target == nullptr || AlignUp(Target->Used, Alignment) + Size > Target->Memory.Size())
    {
        Target = &Slot.emplace_back(Block{.Memory = Buffer(m_Context,
                                                           {.Size   = std::max(m_BlockSize, std::bit_ceil(Size)),
             
                                                            .Memory = MemoryUsage::Stream,
                                                            .Name   = "luvk frame allocator " + std::to_string(m_Current)})});
    }

    const VkDeviceSize Offset = AlignUp(Target->Used, Alignment);
    Target->Used              = Offset + Size;

    return FrameAllocation{.Data    = std::data(Target->Memory.Mapped()) + Offset,
                           .Buffer  = Target->Memory.Handle(),
                           .Offset  = Offset,
                           .Size    = Size,
                           .Address = Target->Memory.Address() + Offset};
}

VkDeviceSize luvk::FrameAllocator::UsedBytes() const noexcept
{
    return std::accumulate(std::begin(m_Slots[m_Current]), std::end(m_Slots[m_Current]), VkDeviceSize{0U},
                           [](const VkDeviceSize Sum, const Block& It)
                           {
                               return Sum + It.Used;
                           });
}
