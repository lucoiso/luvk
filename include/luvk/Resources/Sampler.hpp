// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <volk/volk.h>
#include "luvk/Module.hpp"

namespace luvk
{
    class Device;

    class LUVKMODULE_API Sampler
    {
        VkSampler m_Sampler{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        constexpr Sampler() = default;
        ~Sampler();

        struct CreationArguments
        {
            VkFilter Filter{VK_FILTER_LINEAR};
            VkSamplerAddressMode AddressMode{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        };

        void CreateSampler(std::shared_ptr<Device> const& DeviceModule, CreationArguments const& Arguments);

        [[nodiscard]] constexpr VkSampler const& GetHandle() const
        {
            return m_Sampler;
        }

    private:
        void Destroy();
    };
} // namespace luvk
