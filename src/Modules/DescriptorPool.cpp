// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/DescriptorPool.hpp"
#include <iterator>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Renderer.hpp"

void luvk::DescriptorPool::CreateDescriptorPool(std::shared_ptr<IRenderModule> const& DeviceModule,
                                                std::uint32_t const MaxSets,
                                                std::span<VkDescriptorPoolSize const> PoolSizes,
                                                VkDescriptorPoolCreateFlags const Flags)
{
    m_DeviceModule = DeviceModule;
    auto const Device = std::dynamic_pointer_cast<luvk::Device>(DeviceModule);
    VkDevice const& LogicalDevice = Device->GetLogicalDevice();

    VkDescriptorPoolCreateInfo const Info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                          .flags = Flags,
                                          .maxSets = MaxSets,
                                          .poolSizeCount = static_cast<std::uint32_t>(std::size(PoolSizes)),
                                          .pPoolSizes = std::data(PoolSizes)};

    if (!LUVK_EXECUTE(vkCreateDescriptorPool(LogicalDevice, &Info, nullptr, &m_Pool)))
    {
        throw std::runtime_error("Failed to create descriptor pool.");
    }
}

void luvk::DescriptorPool::InitializeDependencies(std::shared_ptr<IRenderModule> const&)
{
    // No-op
}

void luvk::DescriptorPool::ClearResources()
{
    if (!m_DeviceModule)
    {
        return;
    }

    auto const Device = std::dynamic_pointer_cast<luvk::Device>(m_DeviceModule);
    VkDevice const& LogicalDevice = Device->GetLogicalDevice();

    if (m_Pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(LogicalDevice, m_Pool, nullptr);
        m_Pool = VK_NULL_HANDLE;
    }
}
