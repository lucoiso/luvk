// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    class Context;

    /** Creation parameters for luvk::Sampler. */
    struct SamplerDesc
    {
        /** Magnification filter. */
        VkFilter MagFilter = VK_FILTER_LINEAR;

        /** Minification filter. */
        VkFilter MinFilter = VK_FILTER_LINEAR;

        /** Mipmap selection mode between levels. */
        VkSamplerMipmapMode MipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

        /** Wrap mode on the U axis. */
        VkSamplerAddressMode AddressU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        /** Wrap mode on the V axis. */
        VkSamplerAddressMode AddressV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        /** Wrap mode on the W axis. */
        VkSamplerAddressMode AddressW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

        /** Clamped to the device limit; ignored without samplerAnisotropy. */
        float MaxAnisotropy = 16.F;

        /** Set for shadow (comparison) samplers. */
        std::optional<VkCompareOp> Compare;

        /** MIN / MAX reduce the filtered texels instead of averaging them (e.g. building or reading a depth pyramid);
         *  requires samplerFilterMinmax (enabled when the device supports it). */
        VkSamplerReductionMode Reduction = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE;

        /** Lower LOD clamp. */
        float MinLod = 0.F;

        /** Upper LOD clamp (VK_LOD_CLAMP_NONE disables clamping). */
        float MaxLod = VK_LOD_CLAMP_NONE;

        /** Bias added to the computed LOD. */
        float MipLodBias = 0.F;

        /** Border color used by clamp-to-border addressing. */
        VkBorderColor BorderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

        /** Optional debug name for the sampler. */
        std::string_view Name;
    };

    /** Move-only RAII sampler, registered in the bindless heap (Samplers[] at binding 1). */
    class LUVK_API Sampler
    {
        /** Owning Context used for bindless allocation and retirement. */
        Context* m_Context = nullptr;

        /** Vulkan sampler handle. */
        VkSampler m_Sampler = VK_NULL_HANDLE;

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
        [[nodiscard]] explicit operator bool() const noexcept { return m_Sampler != VK_NULL_HANDLE; }

        /** Underlying VkSampler handle. */
        [[nodiscard]] VkSampler     Handle() const noexcept { return m_Sampler; }

        /** Bindless slot index, or InvalidBindlessIndex. */
        [[nodiscard]] std::uint32_t Index() const noexcept { return m_Index; }
    };
}
