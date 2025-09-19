// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/PipelineCache.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

PipelineCache::~PipelineCache()
{
    Destroy();
}

void PipelineCache::Create(const std::shared_ptr<Device>& DeviceModule)
{
    m_DeviceModule = DeviceModule;
    const VkDevice& Device = m_DeviceModule->GetLogicalDevice();

    constexpr VkPipelineCacheCreateInfo info{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    if (!LUVK_EXECUTE(vkCreatePipelineCache(Device, &info, nullptr, &m_PreRaster)) || !LUVK_EXECUTE(vkCreatePipelineCache(Device, &info, nullptr, &m_Fragment)) || !
        LUVK_EXECUTE(vkCreatePipelineCache(Device, &info, nullptr, &m_Output)) || !LUVK_EXECUTE(vkCreatePipelineCache(Device, &info, nullptr, &m_Composite)))
    {
        Destroy();
        throw std::runtime_error("Failed to create pipeline caches");
    }
}

void PipelineCache::Destroy()
{
    if (m_DeviceModule)
    {
        const VkDevice& Device = m_DeviceModule->GetLogicalDevice();
        if (m_PreRaster != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(Device, m_PreRaster, nullptr);
            m_PreRaster = VK_NULL_HANDLE;
        }
        if (m_Fragment != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(Device, m_Fragment, nullptr);
            m_Fragment = VK_NULL_HANDLE;
        }
        if (m_Output != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(Device, m_Output, nullptr);
            m_Output = VK_NULL_HANDLE;
        }
        if (m_Composite != VK_NULL_HANDLE)
        {
            vkDestroyPipelineCache(Device, m_Composite, nullptr);
            m_Composite = VK_NULL_HANDLE;
        }
    }
    m_DeviceModule.reset();
}
