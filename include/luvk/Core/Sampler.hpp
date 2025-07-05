// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

#include <volk/volk.h>
#include <memory>

namespace luvk
{
    /** Vulkan sampler resource */
    class Device;

    class LUVKMODULE_API Sampler
    {
        /** Handle to the Vulkan sampler */
        VkSampler m_Sampler{VK_NULL_HANDLE};

        /** Device module used to create the sampler */
        std::shared_ptr<Device> m_DeviceModule{};

    public: /** Default constructor */
        constexpr Sampler() = default;

        /** Destructor releases the sampler */
        ~Sampler();

        /** Information used when creating the sampler */
        struct CreationArguments
        {
            /** Filtering mode */
            VkFilter Filter{VK_FILTER_LINEAR};

            /** Addressing mode */
            VkSamplerAddressMode AddressMode{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        };

        /** Create the sampler */
        void CreateSampler(std::shared_ptr<Device> const& DeviceModule, CreationArguments const& Arguments);

        [[nodiscard]] constexpr VkSampler const& GetHandle() const
        {
            return m_Sampler;
        }

    private: /** Destroy the sampler */
        void Destroy();
    };
} // namespace luvk
