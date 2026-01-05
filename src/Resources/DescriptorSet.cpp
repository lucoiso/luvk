/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/DescriptorSet.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

DescriptorSet::DescriptorSet(Device* DeviceModule, DescriptorPool* PoolModule)
    : m_DeviceModule(DeviceModule),
      m_PoolModule(PoolModule) {}

DescriptorSet::~DescriptorSet()
{
    if (m_Layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_DeviceModule->GetLogicalDevice(), m_Layout, nullptr);
    }
}

void DescriptorSet::CreateLayout(std::span<const VkDescriptorSetLayoutBinding> Bindings)
{
    const VkDescriptorSetLayoutCreateInfo Info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                               .bindingCount = static_cast<std::uint32_t>(std::size(Bindings)),
                                               .pBindings = std::data(Bindings)};

    if (!LUVK_EXECUTE(vkCreateDescriptorSetLayout(m_DeviceModule->GetLogicalDevice(), &Info, nullptr, &m_Layout)))
    {
        throw std::runtime_error("Failed to create descriptor set layout");
    }
}

void DescriptorSet::Allocate()
{
    if (!m_PoolModule->AllocateSet(m_Layout, m_Set))
    {
        throw std::runtime_error("Failed to allocate descriptor set");
    }
    m_IsAllocated = true;
}

void DescriptorSet::UpdateUniformBuffer(const std::uint32_t Binding, const VkBuffer Buffer, const VkDeviceSize Range, const VkDeviceSize Offset) const
{
    const VkDescriptorBufferInfo BufferInfo{.buffer = Buffer, .offset = Offset, .range = Range};

    const VkWriteDescriptorSet Write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_Set,
                                     .dstBinding = Binding,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     .pBufferInfo = &BufferInfo};

    vkUpdateDescriptorSets(m_DeviceModule->GetLogicalDevice(), 1, &Write, 0, nullptr);
}

void DescriptorSet::UpdateStorageBuffer(const std::uint32_t Binding, const VkBuffer Buffer, const VkDeviceSize Range, const VkDeviceSize Offset) const
{
    const VkDescriptorBufferInfo BufferInfo{.buffer = Buffer, .offset = Offset, .range = Range};

    const VkWriteDescriptorSet Write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_Set,
                                     .dstBinding = Binding,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                                     .pBufferInfo = &BufferInfo};

    vkUpdateDescriptorSets(m_DeviceModule->GetLogicalDevice(), 1, &Write, 0, nullptr);
}

void DescriptorSet::UpdateImage(const std::uint32_t Binding, const VkImageView ImageView, const VkSampler Sampler, const VkImageLayout Layout) const
{
    const VkDescriptorImageInfo ImageInfo{.sampler = Sampler, .imageView = ImageView, .imageLayout = Layout};

    const VkWriteDescriptorSet Write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_Set,
                                     .dstBinding = Binding,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     .pImageInfo = &ImageInfo};

    vkUpdateDescriptorSets(m_DeviceModule->GetLogicalDevice(), 1, &Write, 0, nullptr);
}
