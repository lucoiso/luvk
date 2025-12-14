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
        VkPipelineCache         m_PreRaster{VK_NULL_HANDLE};
        VkPipelineCache         m_Fragment{VK_NULL_HANDLE};
        VkPipelineCache         m_Output{VK_NULL_HANDLE};
        VkPipelineCache         m_Composite{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        PipelineCache() = delete;
        explicit PipelineCache(const std::shared_ptr<Device>& DeviceModule);

        ~PipelineCache();

        void Create();

        [[nodiscard]] constexpr const VkPipelineCache& GetPreRasterCache() const
        {
            return m_PreRaster;
        }

        [[nodiscard]] constexpr const VkPipelineCache& GetFragmentCache() const
        {
            return m_Fragment;
        }

        [[nodiscard]] constexpr const VkPipelineCache& GetOutputCache() const
        {
            return m_Output;
        }

        [[nodiscard]] constexpr const VkPipelineCache& GetCompositeCache() const
        {
            return m_Composite;
        }
    };
} // namespace luvk
