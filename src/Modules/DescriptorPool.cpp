// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/DescriptorPool.hpp"
#include <iterator>
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

luvk::DescriptorPool::DescriptorPool(const std::shared_ptr<Device>& DeviceModule)
    : m_DeviceModule(DeviceModule) {}

void luvk::DescriptorPool::CreateDescriptorPool(const std::uint32_t                          MaxSets,
                                                const std::span<const VkDescriptorPoolSize>& PoolSizes,
                                                const VkDescriptorPoolCreateFlags            Flags)
{
    const VkDescriptorPoolCreateInfo Info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .flags = Flags,
                                          .maxSets = MaxSets,
                                          .poolSizeCount = static_cast<std::uint32_t>(std::size(PoolSizes)),
                                          .pPoolSizes = std::data(PoolSizes)};

    if (!LUVK_EXECUTE(vkCreateDescriptorPool(m_DeviceModule->GetLogicalDevice(), &Info, nullptr, &m_Pool)))
    {
        throw std::runtime_error("Failed to create descriptor pool.");
    }
}

void luvk::DescriptorPool::ClearResources()
{
    if (m_Pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_DeviceModule->GetLogicalDevice(), m_Pool, nullptr);
        m_Pool = VK_NULL_HANDLE;
    }
}
