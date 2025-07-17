// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include <memory>
#include <volk/volk.h>

namespace luvk
{
    class Device;

    /** Pipeline cache manager splitting caches per pipeline stage */
    class LUVKMODULE_API PipelineCache
    {
        /** Cache used before rasterization stage */
        VkPipelineCache m_PreRaster{VK_NULL_HANDLE};

        /** Cache for fragment processing */
        VkPipelineCache m_Fragment{VK_NULL_HANDLE};

        /** Cache for output stage */
        VkPipelineCache m_Output{VK_NULL_HANDLE};

        /** Cache for composite pipelines */
        VkPipelineCache m_Composite{VK_NULL_HANDLE};

        /** Device that owns the caches */
        std::shared_ptr<Device> m_Device{};

    public: /** Default constructor */
        constexpr PipelineCache() = default;

        /** Destructor releases caches */
        ~PipelineCache();

        /** Create the pipeline caches */
        void Create(std::shared_ptr<Device> const& DeviceModule);

        /** Destroy the pipeline caches */
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
