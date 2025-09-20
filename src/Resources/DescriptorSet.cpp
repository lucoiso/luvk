// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include <cstdint>
#include "luvk/Resources/DescriptorSet.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Modules/Renderer.hpp"

luvk::DescriptorSet::DescriptorSet(const std::shared_ptr<Device>& DeviceModule,
                                   const std::shared_ptr<DescriptorPool>& PoolModule,
                                   const std::shared_ptr<Memory>& MemoryModule)
    : m_DeviceModule(DeviceModule),
      m_PoolModule(PoolModule),
      m_MemoryModule(MemoryModule) {}

luvk::DescriptorSet::~DescriptorSet()
{
    Destroy();
}

void luvk::DescriptorSet::CreateLayout(const LayoutInfo& Info)
{
    m_OwnsLayout = true;

    const VkDescriptorSetLayoutCreateInfo CreateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .bindingCount = static_cast<std::uint32_t>(std::size(Info.Bindings)),
                                                     .pBindings = std::data(Info.Bindings)};

    if (!LUVK_EXECUTE(vkCreateDescriptorSetLayout(m_DeviceModule->GetLogicalDevice(), &CreateInfo, nullptr, &m_Layout)))
    {
        throw std::runtime_error("Failed to create descriptor set layout.");
    }
}

void luvk::DescriptorSet::UseLayout(const VkDescriptorSetLayout& Layout)
{
    m_Layout = Layout;
    m_OwnsLayout = false;
}

void luvk::DescriptorSet::Allocate()
{
    const VkDescriptorSetAllocateInfo AllocateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                   .descriptorPool = m_PoolModule->GetHandle(),
                                                   .descriptorSetCount = 1,
                                                   .pSetLayouts = &m_Layout};

    if (!LUVK_EXECUTE(vkAllocateDescriptorSets(m_DeviceModule->GetLogicalDevice(), &AllocateInfo, &m_Set)))
    {
        throw std::runtime_error("Failed to allocate descriptor set.");
    }
}

void luvk::DescriptorSet::UpdateBuffer(const VkBuffer& Buffer,
                                       const VkDeviceSize Size,
                                       const std::uint32_t Binding,
                                       const VkDescriptorType Type) const
{
    const VkDescriptorBufferInfo BufferInfo{.buffer = Buffer, .offset = 0, .range = Size};

    const VkWriteDescriptorSet Write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_Set,
                                     .dstBinding = Binding,
                                     .descriptorCount = 1,
                                     .descriptorType = Type,
                                     .pBufferInfo = &BufferInfo};

    vkUpdateDescriptorSets(m_DeviceModule->GetLogicalDevice(), 1, &Write, 0, nullptr);
}

void luvk::DescriptorSet::UpdateImage(const VkImageView& View,
                                      const VkSampler& Sampler,
                                      const std::uint32_t Binding,
                                      const VkDescriptorType Type) const
{
    const VkDescriptorImageInfo ImageInfo{.sampler = Sampler,
                                          .imageView = View,
                                          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    const VkWriteDescriptorSet Write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_Set,
                                     .dstBinding = Binding,
                                     .descriptorCount = 1,
                                     .descriptorType = Type,
                                     .pImageInfo = &ImageInfo};

    vkUpdateDescriptorSets(m_DeviceModule->GetLogicalDevice(), 1, &Write, 0, nullptr);
}

void luvk::DescriptorSet::Destroy()
{
    if (m_DeviceModule)
    {
        const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

        if (m_Set != VK_NULL_HANDLE && m_PoolModule)
        {
            const auto& Pool = m_PoolModule;
            vkFreeDescriptorSets(LogicalDevice, Pool->GetHandle(), 1, &m_Set);
            m_Set = VK_NULL_HANDLE;
        }

        if (m_Layout != VK_NULL_HANDLE && m_OwnsLayout)
        {
            vkDestroyDescriptorSetLayout(LogicalDevice, m_Layout, nullptr);
            m_Layout = VK_NULL_HANDLE;
        }
    }
}
