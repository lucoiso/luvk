// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Frame/Feature.hpp"
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <luvk/Core/Context.hpp>

namespace
{
    /** Node of a topological sort: its name, the names it must follow and the names it must precede. */
    struct SortNode
    {
        std::string_view                   Name;
        std::vector<std::string_view>      After;
        std::vector<std::string_view>      Before;
    };

    /**
     * Kahn's algorithm over Nodes; ties keep the input order. Edges to names outside Nodes are ignored. Throws
     * std::logic_error listing the members of a cycle.
     */
    std::vector<std::size_t> TopologicalOrder(const std::vector<SortNode>& Nodes, const std::string_view What)
    {
        const std::size_t Count = std::size(Nodes);
        const auto        Find  = [&](const std::string_view Name) -> std::optional<std::size_t>
        {
            for (std::size_t Index = 0U; Index < Count; ++Index)
            {
                if (Nodes[Index].Name == Name)
                {
                    return Index;
                }
            }
            return std::nullopt;
        };

        std::vector<std::vector<std::size_t>> Next(Count);
        std::vector<std::size_t>              Incoming(Count, 0U);
        const auto                            Link = [&](const std::size_t From, const std::size_t To)
        {
            if (std::ranges::find(Next[From], To) == std::end(Next[From]))
            {
                Next[From].push_back(To);
                ++Incoming[To];
            }
        };

        for (std::size_t Index = 0U; Index < Count; ++Index)
        {
            for (const std::string_view Name : Nodes[Index].After)
            {
                if (const std::optional<std::size_t> Other = Find(Name); Other.has_value() && *Other != Index)
                {
                    Link(*Other, Index);
                }
            }
            for (const std::string_view Name : Nodes[Index].Before)
            {
                if (const std::optional<std::size_t> Other = Find(Name); Other.has_value() && *Other != Index)
                {
                    Link(Index, *Other);
                }
            }
        }

        std::vector<std::size_t> Order;
        Order.reserve(Count);
        std::vector<bool> Done(Count, false);
        while (std::size(Order) < Count)
        {
            bool Progress = false;
            for (std::size_t Index = 0U; Index < Count; ++Index)
            {
                if (!Done[Index] && Incoming[Index] == 0U)
                {
                    Done[Index] = true;
                    Order.push_back(Index);
                    for (const std::size_t To : Next[Index])
                    {
                        --Incoming[To];
                    }
                    Progress = true;
                    break;
                }
            }

            if (!Progress)
            {
                std::string Members;
                for (std::size_t Index = 0U; Index < Count; ++Index)
                {
                    if (!Done[Index])
                    {
                        Members += (std::empty(Members) ? "" : ", ") + std::string(Nodes[Index].Name);
                    }
                }
                throw std::logic_error("[luvk] Cyclic " + std::string(What) + " order between: " + Members + ".");
            }
        }
        return Order;
    }
}

luvk::frame::RenderFeature::~RenderFeature() = default;

std::span<const std::string_view> luvk::frame::RenderFeature::After() const
{
    return {};
}

std::span<const std::string_view> luvk::frame::RenderFeature::Before() const
{
    return {};
}

void luvk::frame::RenderFeature::OnAttach(Context&)
{
}

void luvk::frame::RenderFeature::OnDetach(Context&)
{
}

void luvk::frame::RenderFeature::OnResize(Context&, const FrameSettings&)
{
}

void luvk::frame::RenderFeature::Update(float)
{
}

void luvk::frame::RenderFeature::OnGui()
{
}

void luvk::frame::RenderFeature::SetEnabled(const bool Enabled) noexcept
{
    m_Enabled = Enabled;
    if (Enabled)
    {
        m_Failure.clear();
    }
}

luvk::frame::FeatureStack::FeatureStack(Context& Ctx)
    : m_Context(Ctx)
{
    constexpr std::string_view Defaults[] = {Phases::Prepare, Phases::Compute, Phases::Opaque, Phases::Transparent, Phases::Debug,
                                             Phases::PostProcess, Phases::Upscale, Phases::Overlay, Phases::Output};
    for (std::size_t Index = 0U; Index < std::size(Defaults); ++Index)
    {
        PhaseDesc Phase{.Name = std::string(Defaults[Index])};
        if (Index > 0U)
        {
            Phase.After.emplace_back(Defaults[Index - 1U]);
        }
        m_Phases.push_back(std::move(Phase));
    }
    Sort();
}

luvk::frame::FeatureStack::~FeatureStack()
{
    Clear();
}

void luvk::frame::FeatureStack::Sort()
{
    std::vector<SortNode> PhaseNodes;
    PhaseNodes.reserve(std::size(m_Phases));
    for (const PhaseDesc& Phase : m_Phases)
    {
        SortNode Node{.Name = Phase.Name};
        Node.After.assign(std::begin(Phase.After), std::end(Phase.After));
        Node.Before.assign(std::begin(Phase.Before), std::end(Phase.Before));
        PhaseNodes.push_back(std::move(Node));
    }

    std::vector<std::string> PhaseOrder;
    for (const std::size_t Index : TopologicalOrder(PhaseNodes, "phase"))
    {
        PhaseOrder.push_back(m_Phases[Index].Name);
    }

    std::vector<std::shared_ptr<RenderFeature>> Features;
    std::vector<std::uint32_t>                  FeaturePhase;
    for (std::uint32_t Phase = 0U; Phase < std::size(PhaseOrder); ++Phase)
    {
        std::vector<std::size_t> Members;
        std::vector<SortNode>    Nodes;
        for (std::size_t Index = 0U; Index < std::size(m_Added); ++Index)
        {
            const RenderFeature& Feature = *m_Added[Index];
            if (Feature.Phase() != PhaseOrder[Phase])
            {
                continue;
            }
            const std::span<const std::string_view> After  = Feature.After();
            const std::span<const std::string_view> Before = Feature.Before();
            Members.push_back(Index);
            Nodes.push_back({.Name = Feature.Name(), .After = {std::begin(After), std::end(After)}, .Before = {std::begin(Before), std::end(Before)}});
        }

        for (const std::size_t Index : TopologicalOrder(Nodes, "feature"))
        {
            Features.push_back(m_Added[Members[Index]]);
            FeaturePhase.push_back(Phase);
        }
    }

    if (std::size(Features) != std::size(m_Added))
    {
        for (const auto& Feature : m_Added)
        {
            if (std::ranges::find(PhaseOrder, Feature->Phase()) == std::end(PhaseOrder))
            {
                throw std::logic_error("[luvk] Feature '" + std::string(Feature->Name()) + "' uses the unknown phase '" + std::string(Feature->Phase()) + "'.");
            }
        }
    }

    m_PhaseOrder   = std::move(PhaseOrder);
    m_Features     = std::move(Features);
    m_FeaturePhase = std::move(FeaturePhase);
}

std::uint32_t luvk::frame::FeatureStack::PhaseIndex(const std::string_view Name) const
{
    const auto It = std::ranges::find(m_PhaseOrder, Name);
    if (It == std::end(m_PhaseOrder))
    {
        throw std::invalid_argument("[luvk] Unknown phase '" + std::string(Name) + "'.");
    }
    return static_cast<std::uint32_t>(std::distance(std::begin(m_PhaseOrder), It));
}

template <typename Callback>
void luvk::frame::FeatureStack::Guard(RenderFeature& Feature, const std::string_view Where, Callback&& Function) const
{
    try
    {
        std::forward<Callback>(Function)();
    }
    catch (const std::exception& Error)
    {
        Feature.m_Failure = Error.what();
        m_Context.EmitMessage(DebugSeverity::Error,
                              "[luvk] Feature '" + std::string(Feature.Name()) + "' was disabled: " + std::string(Where) + " threw: " + Feature.m_Failure);
    }
}

void luvk::frame::FeatureStack::AddPhase(PhaseDesc Phase)
{
    if (std::ranges::any_of(m_Phases, [&](const PhaseDesc& Existing) { return Existing.Name == Phase.Name; }))
    {
        throw std::invalid_argument("[luvk] Phase '" + Phase.Name + "' already exists.");
    }

    m_Phases.push_back(std::move(Phase));
    try
    {
        Sort();
    }
    catch (...)
    {
        m_Phases.pop_back();
        Sort();
        throw;
    }
}

void luvk::frame::FeatureStack::Add(std::shared_ptr<RenderFeature> Feature)
{
    if (!Feature)
    {
        throw std::invalid_argument("[luvk] FeatureStack::Add called with a null feature.");
    }

    Feature->OnAttach(m_Context);
    try
    {
        if (m_Settings.has_value())
        {
            // Features added after the first frame (or from OnInit, after the settings were computed) still get them.
            Feature->OnResize(m_Context, *m_Settings);
        }

        m_Added.push_back(Feature);
        Sort();
    }
    catch (...)
    {
        if (!std::empty(m_Added) && m_Added.back() == Feature)
        {
            m_Added.pop_back();
            Sort();
        }
        Feature->OnDetach(m_Context);
        throw;
    }
}

void luvk::frame::FeatureStack::Remove(const RenderFeature& Feature)
{
    const auto It = std::ranges::find_if(m_Added, [&](const std::shared_ptr<RenderFeature>& Candidate) { return Candidate.get() == &Feature; });
    if (It != std::end(m_Added))
    {
        const std::shared_ptr<RenderFeature> Removed = *It;
        m_Added.erase(It);
        Sort();
        Removed->OnDetach(m_Context);
    }
}

void luvk::frame::FeatureStack::Clear()
{
    for (auto It = std::rbegin(m_Features); It != std::rend(m_Features); ++It)
    {
        (*It)->OnDetach(m_Context);
    }
    m_Features.clear();
    m_FeaturePhase.clear();
    m_Added.clear();
}

void luvk::frame::FeatureStack::Update(const float DeltaTime) const
{
    for (const auto& Feature : m_Features)
    {
        if (Feature->IsEnabled())
        {
            Guard(*Feature, "Update", [&] { Feature->Update(DeltaTime); });
        }
    }
}

void luvk::frame::FeatureStack::Resize(const FrameSettings& Settings)
{
    m_Settings = Settings;
    for (const auto& Feature : m_Features)
    {
        Feature->OnResize(m_Context, Settings);
    }
}

void luvk::frame::FeatureStack::AddPasses(FeatureContext& Frame) const
{
    for (const auto& Feature : m_Features)
    {
        if (Feature->IsEnabled())
        {
            Guard(*Feature, "AddPasses", [&] { Feature->AddPasses(Frame); });
        }
    }
}

void luvk::frame::FeatureStack::AddPasses(FeatureContext& Frame, const std::string_view First, const std::string_view Last) const
{
    const std::uint32_t From = PhaseIndex(First);
    const std::uint32_t To   = PhaseIndex(Last);
    for (std::size_t Index = 0U; Index < std::size(m_Features); ++Index)
    {
        RenderFeature& Feature = *m_Features[Index];
        if (Feature.IsEnabled() && m_FeaturePhase[Index] >= From && m_FeaturePhase[Index] <= To)
        {
            Guard(Feature, "AddPasses", [&] { Feature.AddPasses(Frame); });
        }
    }
}
