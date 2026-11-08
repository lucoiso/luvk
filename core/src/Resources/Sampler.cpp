// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Sampler.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Vulkan.hpp"

luvk::Sampler::Sampler(Context& Ctx, const SamplerDesc& Desc)
    : m_Context(&Ctx)
{
    const bool AnisotropySupported = Ctx.EnabledFeatures().Core().samplerAnisotropy != VK_FALSE;
    const bool UseAnisotropy       = AnisotropySupported && Desc.MaxAnisotropy > 1.F;

    const bool Reduces = Desc.Reduction != ReductionMode::WeightedAverage;
    if (Reduces && Ctx.EnabledFeatures().Vulkan12().samplerFilterMinmax == VK_FALSE)
    {
        throw std::logic_error("[luvk] Min/max sampler reduction requires samplerFilterMinmax (not supported by this device).");
    }

    const VkSamplerReductionModeCreateInfo ReductionInfo{.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO, .reductionMode = ToVk(Desc.Reduction)};

    const VkSamplerCreateInfo Info{.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                                   .pNext                   = Reduces ? &ReductionInfo : nullptr,
                                   .magFilter               = ToVk(Desc.MagFilter),
                                   .minFilter               = ToVk(Desc.MinFilter),
                                   .mipmapMode              = ToVk(Desc.MipmapMode),
                                   .addressModeU            = ToVk(Desc.AddressU),
                                   .addressModeV            = ToVk(Desc.AddressV),
                                   .addressModeW            = ToVk(Desc.AddressW),
                                   .mipLodBias              = Desc.MipLodBias,
                                   .anisotropyEnable        = UseAnisotropy ? VK_TRUE : VK_FALSE,
                                   .maxAnisotropy           = UseAnisotropy ? std::min(Desc.MaxAnisotropy, Ctx.Properties().limits.maxSamplerAnisotropy) : 1.F,
                                   .compareEnable           = Desc.Compare.has_value() ? VK_TRUE : VK_FALSE,
                                   .compareOp               = ToVk(Desc.Compare.value_or(CompareOp::Always)),
                                   .minLod                  = Desc.MinLod,
                                   .maxLod                  = Desc.MaxLod,
                                   .borderColor             = ToVk(Desc.BorderColor),
                                   .unnormalizedCoordinates = VK_FALSE};

    Check(vkCreateSampler(Ctx.Device(), &Info, nullptr, &m_Sampler), "vkCreateSampler");
    Ctx.TrackResource(+1);

    try
    {
        m_Index = Ctx.Bindless().Allocate(BindlessSlot::Sampler);
        Ctx.Bindless().WriteSampler(m_Index, m_Sampler);
    }
    catch (...)
    {
        Reset();
        throw;
    }

    if (!std::empty(Desc.Name))
    {
        Ctx.SetDebugName(m_Sampler, VK_OBJECT_TYPE_SAMPLER, Desc.Name);
    }
}

luvk::Sampler::~Sampler()
{
    Reset();
}

luvk::Sampler::Sampler(Sampler&& Other) noexcept
    : m_Context(std::exchange(Other.m_Context, nullptr)),
      m_Sampler(std::exchange(Other.m_Sampler, VK_NULL_HANDLE)),
      m_Index(std::exchange(Other.m_Index, InvalidBindlessIndex)) {}

luvk::Sampler& luvk::Sampler::operator=(Sampler&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        m_Context = std::exchange(Other.m_Context, nullptr);
        m_Sampler = std::exchange(Other.m_Sampler, VK_NULL_HANDLE);
        m_Index   = std::exchange(Other.m_Index, InvalidBindlessIndex);
    }
    return *this;
}

void luvk::Sampler::Reset() noexcept
{
    if (m_Context == nullptr)
    {
        return;
    }

    if (m_Index != InvalidBindlessIndex)
    {
        m_Context->Retire({.Kind = RetiredKind::BindlessSlot, .Slot = static_cast<std::uint32_t>(BindlessSlot::Sampler), .Index = m_Index});
        m_Index = InvalidBindlessIndex;
    }

    if (m_Sampler != VK_NULL_HANDLE)
    {
        m_Context->Retire({.Kind = RetiredKind::Sampler, .Handle = HandleToU64(m_Sampler)});
        m_Context->TrackResource(-1);
        m_Sampler = VK_NULL_HANDLE;
    }
}
