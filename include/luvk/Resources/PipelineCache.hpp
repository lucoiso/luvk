// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <volk.h>

namespace luvk
{
    class Device;

    class LUVK_API PipelineCache
    {
    protected:
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

        [[nodiscard]] constexpr VkPipelineCache GetPreRasterCache() const noexcept
        {
            return m_PreRaster;
        }

        [[nodiscard]] constexpr VkPipelineCache GetFragmentCache() const noexcept
        {
            return m_Fragment;
        }

        [[nodiscard]] constexpr VkPipelineCache GetOutputCache() const noexcept
        {
            return m_Output;
        }

        [[nodiscard]] constexpr VkPipelineCache GetCompositeCache() const noexcept
        {
            return m_Composite;
        }
    };
} // namespace luvk
