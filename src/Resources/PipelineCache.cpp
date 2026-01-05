/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/PipelineCache.hpp"
#include <fstream>
#include <stdexcept>
#include <vector>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

PipelineCache::PipelineCache(Device* DeviceModule)
    : m_DeviceModule(DeviceModule) {}

PipelineCache::~PipelineCache()
{
    if (m_Cache != VK_NULL_HANDLE)
    {
        vkDestroyPipelineCache(m_DeviceModule->GetLogicalDevice(), m_Cache, nullptr);
    }
}

void PipelineCache::Initialize()
{
    constexpr VkPipelineCacheCreateInfo Info{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    if (!LUVK_EXECUTE(vkCreatePipelineCache(m_DeviceModule->GetLogicalDevice(), &Info, nullptr, &m_Cache)))
    {
        throw std::runtime_error("Failed to initialize empty pipeline cache");
    }
}

void PipelineCache::Save(const std::string_view Path) const
{
    if (m_Cache == VK_NULL_HANDLE)
    {
        return;
    }

    const VkDevice Device = m_DeviceModule->GetLogicalDevice();
    std::size_t Size = 0;

    if (vkGetPipelineCacheData(Device, m_Cache, &Size, nullptr) != VK_SUCCESS)
    {
        return;
    }

    std::vector<char> Data(Size);
    if (vkGetPipelineCacheData(Device, m_Cache, &Size, std::data(Data)) != VK_SUCCESS)
    {
        return;
    }

    std::ofstream File(std::data(Path), std::ios::binary);
    if (File.is_open())
    {
        File.write(std::data(Data), static_cast<std::streamsize>(Size));
    }
}

void PipelineCache::Load(const std::string_view Path)
{
    std::vector<char> Data;
    std::ifstream File(std::data(Path), std::ios::binary | std::ios::ate);

    if (File.is_open())
    {
        const auto FileSize = File.tellg();
        if (FileSize > 0)
        {
            Data.resize(FileSize);
            File.seekg(0, std::ios::beg);
            File.read(std::data(Data), FileSize);
        }
    }

    const VkPipelineCacheCreateInfo Info{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
                                         .initialDataSize = std::size(Data),
                                         .pInitialData = std::empty(Data) ? nullptr : std::data(Data)};

    if (m_Cache != VK_NULL_HANDLE)
    {
        vkDestroyPipelineCache(m_DeviceModule->GetLogicalDevice(), m_Cache, nullptr);
    }

    if (!LUVK_EXECUTE(vkCreatePipelineCache(m_DeviceModule->GetLogicalDevice(), &Info, nullptr, &m_Cache)))
    {
        constexpr VkPipelineCacheCreateInfo EmptyInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
        vkCreatePipelineCache(m_DeviceModule->GetLogicalDevice(), &EmptyInfo, nullptr, &m_Cache);
    }
}
