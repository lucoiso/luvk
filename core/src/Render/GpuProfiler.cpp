// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Render/GpuProfiler.hpp"
#include <algorithm>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"

namespace
{
    constexpr std::uint32_t InvalidToken = 0xFFFFFFFFU;
    constexpr double        Smoothing    = 0.1;

    void Accumulate(luvk::PassTiming& Timing, const double Value)
    {
        Timing.Milliseconds = Value;
        Timing.Average      = Timing.Average == 0.0 ? Value : Timing.Average + (Value - Timing.Average) * Smoothing;
    }
}

luvk::GpuProfiler::GpuProfiler(Context& Ctx, const std::uint32_t SlotCount, const std::uint32_t MaxPasses)
    : m_Context(Ctx),
      m_MaxQueries(MaxPasses * 2U + 2U),
      m_PeriodNs(Ctx.Properties().limits.timestampPeriod)
{
    const auto Families = Enumerate<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, Ctx.PhysicalDevice());
    m_Supported         = Ctx.GraphicsQueue().Family() < std::size(Families) &&
                          Families[Ctx.GraphicsQueue().Family()].timestampValidBits > 0U &&
                          m_PeriodNs > 0.0;
    if (!m_Supported)
    {
        return;
    }

    const std::uint32_t ValidBits = Families[Ctx.GraphicsQueue().Family()].timestampValidBits;
    m_TimestampMask               = ValidBits >= 64U ? ~std::uint64_t{0U} : (std::uint64_t{1U} << ValidBits) - 1U;

    m_Slots.resize(std::max(1U, SlotCount));

    const VkQueryPoolCreateInfo Info{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = m_MaxQueries};
    for (Slot& It : m_Slots)
    {
        Check(vkCreateQueryPool(Ctx.Device(), &Info, nullptr, &It.Pool), "vkCreateQueryPool (timestamps)");
    }
}

luvk::GpuProfiler::~GpuProfiler()
{
    for (const Slot& It : m_Slots)
    {
        if (It.Pool != VK_NULL_HANDLE)
        {

            vkDestroyQueryPool(m_Context.Device(), It.Pool, nullptr);
        }
    }
}

void luvk::GpuProfiler::BeginFrame(const VkCommandBuffer Commands, const std::uint32_t SlotIndex)
{
    if (!m_Supported)
    {
        return;
    }

    m_Current     = SlotIndex % static_cast<std::uint32_t>(std::size(m_Slots));
    Slot& Current = m_Slots[m_Current];
    Collect(Current);

    vkCmdResetQueryPool(Commands, Current.Pool, 0U, m_MaxQueries);
    Current.Names.clear();
    Current.Queries = 2U;
    vkCmdWriteTimestamp2(Commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, Current.Pool, 0U);
}

void luvk::GpuProfiler::EndFrame(const VkCommandBuffer Commands)
{
    if (!m_Supported)
    {
        return;
    }

    Slot& Current = m_Slots[m_Current];
    vkCmdWriteTimestamp2(Commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, Current.Pool, 1U);
    Current.Pending = true;
}

std::uint32_t luvk::GpuProfiler::BeginPass(const VkCommandBuffer Commands, const std::string_view Name)
{
    if (!m_Supported)
    {
        return InvalidToken;
    }

    Slot& Current = m_Slots[m_Current];
    if (Current.Queries + 2U > m_MaxQueries)
    {
        return InvalidToken;
    }

    const std::uint32_t Query = Current.Queries;
    Current.Queries += 2U;
    Current.Names.emplace_back(Name);
    vkCmdWriteTimestamp2(Commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, Current.Pool, Query);
    return Query;
}

void luvk::GpuProfiler::EndPass(const VkCommandBuffer Commands, const std::uint32_t Token)
{
    if (Token == InvalidToken)
    {
        return;
    }
    vkCmdWriteTimestamp2(Commands, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, m_Slots[m_Current].Pool, Token + 1U);
}

void luvk::GpuProfiler::Collect(Slot& Target)
{
    if (!Target.Pending || Target.Queries == 0U)
    {
        return;
    }
    Target.Pending = false;

    std::vector<std::uint64_t> Values(Target.Queries);
    const VkResult             Result = vkGetQueryPoolResults(m_Context.Device(),
                                                  Target.Pool,
                                                  0U,
                                                  Target.Queries,
                                                  std::size(Values) * sizeof(std::uint64_t),
                                                  std::data(Values),
                                                  sizeof(std::uint64_t),
                                                  VK_QUERY_RESULT_64_BIT);
    if (Result != VK_SUCCESS)
    {
        return;
    }

    // Only the low timestampValidBits are meaningful; masking the difference also survives a counter wrap.
    const auto ToMs = [this](const std::uint64_t Begin, const std::uint64_t End)
    {
        return static_cast<double>((End - Begin) & m_TimestampMask) * m_PeriodNs / 1.0e6;
    };

    Accumulate(m_Frame, ToMs(Values[0], Values[1]));

    // Passes sharing a name (e.g. one pass per UI window) are summed into one entry, then averaged once per frame.
    std::vector<PassTiming> FrameTimings;
    for (std::size_t Index = 0U; Index < std::size(Target.Names); ++Index)
    {
        const std::string& Name   = Target.Names[Index];
        const double       Millis = ToMs(Values[2U + Index * 2U], Values[3U + Index * 2U]);

        auto It = std::ranges::find(FrameTimings, Name, &PassTiming::Name);
        if (It == std::end(FrameTimings))
        {
            FrameTimings.push_back(PassTiming{.Name = Name});
            It = std::prev(std::end(FrameTimings));
        }
        It->Milliseconds += Millis;
    }

    for (const PassTiming& Timing : FrameTimings)
    {
        auto It = std::ranges::find(m_Results, Timing.Name, &PassTiming::Name);
        if (It == std::end(m_Results))
        {
            m_Results.push_back(PassTiming{.Name = Timing.Name});
            It = std::prev(std::end(m_Results));
        }
        Accumulate(*It, Timing.Milliseconds);
    }

    std::erase_if(m_Results,
                  [&](const PassTiming& Timing)
                  {
                      return std::ranges::find(Target.Names, Timing.Name) == std::end(Target.Names);
                  });
}
