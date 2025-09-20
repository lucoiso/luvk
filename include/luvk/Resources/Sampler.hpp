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
        Sampler() = delete;
        explicit Sampler(const std::shared_ptr<Device>& DeviceModule);

        ~Sampler();

        struct CreationArguments
        {
            VkFilter Filter{VK_FILTER_LINEAR};
            VkSamplerAddressMode AddressMode{VK_SAMPLER_ADDRESS_MODE_REPEAT};
        };

        void CreateSampler(const CreationArguments& Arguments);

        [[nodiscard]] constexpr const VkSampler& GetHandle() const
        {
            return m_Sampler;
        }

    private:
        void Destroy();
    };
} // namespace luvk
