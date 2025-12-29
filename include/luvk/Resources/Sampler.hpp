/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <volk.h>

namespace luvk
{
    class Device;

    /**
     * Arguments for Vulkan sampler creation.
     */
    struct LUVK_API SamplerCreationArguments
    {
        /** Minification and magnification filter. */
        VkFilter Filter{VK_FILTER_LINEAR};

        /** Tiling mode for texture coordinates. */
        VkSamplerAddressMode AddressMode{VK_SAMPLER_ADDRESS_MODE_REPEAT};

        /** Mipmap selection mode. */
        VkSamplerMipmapMode MipmapMode{VK_SAMPLER_MIPMAP_MODE_LINEAR};

        /** Maximum anisotropy value (1.0 is default). */
        float MaxAnisotropy{1.F};
    };

    /**
     * Wrapper for a Vulkan Sampler.
     */
    class LUVK_API Sampler
    {
        /** The Vulkan sampler handle. */
        VkSampler m_Sampler{VK_NULL_HANDLE};

        /** Pointer to the Device module for logical device handle. */
        Device* m_DeviceModule{nullptr};

    public:
        /** Samplers cannot be default constructed. */
        Sampler() = delete;

        /**
         * Constructor.
         * @param DeviceModule Pointer to the Device module.
         */
        explicit Sampler(Device* DeviceModule);

        /** Destructor (destroys the sampler). */
        ~Sampler();

        /**
         * Creates or recreates the sampler.
         * @param Arguments Configuration for the sampler.
         */
        void CreateSampler(const SamplerCreationArguments& Arguments);

        /** Get the underlying VkSampler handle. */
        [[nodiscard]] constexpr VkSampler GetHandle() const noexcept
        {
            return m_Sampler;
        }
    };
}
