// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/DescriptorPool.hpp"
#include <iterator>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Renderer.hpp"

void luvk::DescriptorPool::CreateDescriptorPool(const std::shared_ptr<Device>& DeviceModule,
                                                const std::uint32_t MaxSets,
                                                const std::span<const VkDescriptorPoolSize>& PoolSizes,
                                                const VkDescriptorPoolCreateFlags Flags)
{
    m_DeviceModule = DeviceModule;
    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

    const VkDescriptorPoolCreateInfo Info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .flags = Flags,
                                          .maxSets = MaxSets,
                                          .poolSizeCount = static_cast<std::uint32_t>(std::size(PoolSizes)),
                                          .pPoolSizes = std::data(PoolSizes)};

    if (!LUVK_EXECUTE(vkCreateDescriptorPool(LogicalDevice, &Info, nullptr, &m_Pool)))
    {
        throw std::runtime_error("Failed to create descriptor pool.");
    }
}

void luvk::DescriptorPool::InitializeDependencies(const std::shared_ptr<IRenderModule>&)
{
    // No-op
}

void luvk::DescriptorPool::ClearResources()
{
    if (!m_DeviceModule)
    {
        return;
    }

    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

    if (m_Pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(LogicalDevice, m_Pool, nullptr);
        m_Pool = VK_NULL_HANDLE;
    }
}
