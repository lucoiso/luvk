// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/App/Feature.hpp"
#include <algorithm>
#include <stdexcept>

luvk::app::RenderFeature::~RenderFeature() = default;

void luvk::app::RenderFeature::OnAttach(Context&)
{
}

void luvk::app::RenderFeature::OnDetach(Context&)
{
}

void luvk::app::RenderFeature::OnResize(Context&, const FrameSettings&)
{
}

void luvk::app::RenderFeature::Update(float)
{
}

void luvk::app::RenderFeature::OnGui()
{
}

void luvk::app::RenderFeature::SetEnabled(const bool Enabled) noexcept
{
    m_Enabled = Enabled;
}

std::string_view luvk::app::Blackboard::Intern(const std::string_view Name)
{
    return *m_Names.emplace(Name).first;
}

void luvk::app::Blackboard::Clear()
{
    m_Textures.clear();
    m_Buffers.clear();
    m_Data.clear();
    m_Names.clear();
}

void luvk::app::Blackboard::SetTexture(const std::string_view Name, const TextureHandle Texture)
{
    m_Textures.insert_or_assign(Intern(Name), Texture);
}

void luvk::app::Blackboard::SetBuffer(const std::string_view Name, const BufferHandle Buffer)
{
    m_Buffers.insert_or_assign(Intern(Name), Buffer);
}

luvk::TextureHandle luvk::app::Blackboard::FindTexture(const std::string_view Name) const
{
    const auto It = m_Textures.find(Name);
    return It != std::end(m_Textures) ? It->second : TextureHandle{};
}

luvk::BufferHandle luvk::app::Blackboard::FindBuffer(const std::string_view Name) const
{
    const auto It = m_Buffers.find(Name);
    return It != std::end(m_Buffers) ? It->second : BufferHandle{};
}

luvk::app::FeatureStack::FeatureStack(Context& Ctx)
    : m_Context(Ctx) {}

luvk::app::FeatureStack::~FeatureStack()
{
    Clear();
}

void luvk::app::FeatureStack::Add(std::shared_ptr<RenderFeature> Feature)
{
    if (!Feature)
    {
        throw std::invalid_argument("[luvk] FeatureStack::Add called with a null feature.");
    }

    Feature->OnAttach(m_Context);
    if (m_Settings.has_value())
    {
        // Features added after the first frame (or from OnInit, after the settings were computed) still get them.
        Feature->OnResize(m_Context, *m_Settings);
    }

    const auto Position = std::ranges::upper_bound(m_Features,
                                                   Feature->Stage(),
                                                   std::less{},
                                                   [](const std::shared_ptr<RenderFeature>& It)
                                                   {
                                                       return It->Stage();
                                                   });
    m_Features.insert(Position, std::move(Feature));
}

void luvk::app::FeatureStack::Remove(const RenderFeature& Feature)
{
    const auto It = std::ranges::find_if(m_Features,
                                         [&](const std::shared_ptr<RenderFeature>& Candidate)
                                         {
                                             return Candidate.get() == &Feature;
                                         });
    if (It != std::end(m_Features))
    {
        (*It)->OnDetach(m_Context);
        m_Features.erase(It);
    }
}

void luvk::app::FeatureStack::Clear()
{
    for (auto It = std::rbegin(m_Features); It != std::rend(m_Features); ++It)
    {
        (*It)->OnDetach(m_Context);
    }
    m_Features.clear();
}

void luvk::app::FeatureStack::Update(const float DeltaTime) const
{
    for (const auto& Feature : m_Features)
    {
        if (Feature->IsEnabled())
        {
            Feature->Update(DeltaTime);
        }
    }
}

void luvk::app::FeatureStack::Resize(const FrameSettings& Settings)
{
    m_Settings = Settings;
    for (const auto& Feature : m_Features)
    {
        Feature->OnResize(m_Context, Settings);
    }
}

void luvk::app::FeatureStack::AddPasses(FeatureContext& Frame) const
{
    AddPasses(Frame, FeatureStage::Prepare, FeatureStage::Output);
}

void luvk::app::FeatureStack::AddPasses(FeatureContext& Frame, const FeatureStage First, const FeatureStage Last) const
{
    for (const auto& Feature : m_Features)
    {
        if (Feature->IsEnabled() && Feature->Stage() >= First && Feature->Stage() <= Last)
        {
            Feature->AddPasses(Frame);
        }
    }
}
