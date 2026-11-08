// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include "luvk/Core/Common.hpp"
#include "luvk/Core/Handles.hpp"

namespace luvk
{
    class Context;

    /** Creation parameters for luvk::Sampler. */
    struct SamplerDesc
    {
        /** Magnification filter. */
        Filter MagFilter = Filter::Linear;

        /** Minification filter. */
        Filter MinFilter = Filter::Linear;

        /** Mipmap selection mode between levels. */
        luvk::MipmapMode MipmapMode = luvk::MipmapMode::Linear;

        /** Wrap mode on the U axis. */
        AddressMode AddressU = AddressMode::Repeat;

        /** Wrap mode on the V axis. */
        AddressMode AddressV = AddressMode::Repeat;

        /** Wrap mode on the W axis. */
        AddressMode AddressW = AddressMode::Repeat;

        /** Clamped to the device limit; ignored without samplerAnisotropy. */
        float MaxAnisotropy = 16.F;

        /** Set for shadow (comparison) samplers. */
        std::optional<CompareOp> Compare;

        /** MIN / MAX reduce the filtered texels instead of averaging them (e.g. building or reading a depth pyramid);
         *  requires samplerFilterMinmax (enabled when the device supports it). */
        ReductionMode Reduction = ReductionMode::WeightedAverage;

        /** Lower LOD clamp. */
        float MinLod = 0.F;

        /** Upper LOD clamp (LodClampNone disables clamping). */
        float MaxLod = LodClampNone;

        /** Bias added to the computed LOD. */
        float MipLodBias = 0.F;

        /** Border color used by clamp-to-border addressing. */
        luvk::BorderColor BorderColor = luvk::BorderColor::FloatTransparentBlack;

        /** Optional debug name for the sampler. */
        std::string_view Name;
    };

    /** Move-only RAII sampler, registered in the bindless heap (Samplers[] at binding 1). */
    class LUVK_API Sampler
    {
        /** Owning Context used for bindless allocation and retirement. */
        Context* m_Context = nullptr;

        /** Vulkan sampler handle. */
        VkSampler m_Sampler = nullptr;

        /** Bindless slot in Samplers[] (binding 1), or InvalidBindlessIndex. */
        std::uint32_t m_Index = InvalidBindlessIndex;

    public:
        /** Default constructor. */
        Sampler() = default;

        /** Constructor. Creates the VkSampler and allocates its bindless slot (Samplers[] at binding 1). */
        Sampler(Context& Ctx, const SamplerDesc& Desc);

        /** Destructor. */
        ~Sampler();

        /** Non-copyable. */
        Sampler(const Sampler&)            = delete;

        /** Non-copyable. */
        Sampler& operator=(const Sampler&) = delete;

        /** Move-only. */
        Sampler(Sampler&& Other) noexcept;

        /** Move-only. */
        Sampler& operator=(Sampler&& Other) noexcept;

        /** Retires the sampler and its bindless slot (deferred) and leaves this object empty. */
        void Reset() noexcept;

        /** True when a VkSampler was created. */
        [[nodiscard]] explicit operator bool() const noexcept { return m_Sampler != nullptr; }

        /** Underlying VkSampler handle. */
        [[nodiscard]] VkSampler     Handle() const noexcept { return m_Sampler; }

        /** Bindless slot index, or InvalidBindlessIndex. */
        [[nodiscard]] std::uint32_t Index() const noexcept { return m_Index; }

        /** Typed bindless handle of the sampler. */
        [[nodiscard]] SamplerHandle Bindless() const noexcept { return {m_Index}; }
    };
}
