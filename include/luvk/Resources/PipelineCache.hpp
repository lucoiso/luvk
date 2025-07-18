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

    class LUVKMODULE_API PipelineCache
    {
        VkPipelineCache m_PreRaster{VK_NULL_HANDLE};
        VkPipelineCache m_Fragment{VK_NULL_HANDLE};
        VkPipelineCache m_Output{VK_NULL_HANDLE};
        VkPipelineCache m_Composite{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_Device{};

    public:
        constexpr PipelineCache() = default;
        ~PipelineCache();

        void Create(std::shared_ptr<Device> const& DeviceModule);
        void Destroy();

        [[nodiscard]] constexpr VkPipelineCache GetPreRasterCache() const
        {
            return m_PreRaster;
        }

        [[nodiscard]] constexpr VkPipelineCache GetFragmentCache() const
        {
            return m_Fragment;
        }

        [[nodiscard]] constexpr VkPipelineCache GetOutputCache() const
        {
            return m_Output;
        }

        [[nodiscard]] constexpr VkPipelineCache GetCompositeCache() const
        {
            return m_Composite;
        }
    };
} // namespace luvk
