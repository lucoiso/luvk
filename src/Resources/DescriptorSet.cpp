// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Resources/DescriptorSet.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"

luvk::DescriptorSet::DescriptorSet(const std::shared_ptr<Device>&         DeviceModule,
                                   const std::shared_ptr<DescriptorPool>& PoolModule,
                                   const std::shared_ptr<Memory>&         MemoryModule)
    : m_DeviceModule(DeviceModule),
      m_PoolModule(PoolModule),
      m_MemoryModule(MemoryModule) {}

luvk::DescriptorSet::DescriptorSet(const std::shared_ptr<Device>&         DeviceModule,
                                   const std::shared_ptr<DescriptorPool>& PoolModule,
                                   const std::shared_ptr<Memory>&         MemoryModule,
                                   const VkDescriptorSet                  ExistingSet,
                                   const VkDescriptorSetLayout            ExistingLayout)
    : m_Owned(false),
      m_Layout(ExistingLayout),
      m_Set(ExistingSet),
      m_DeviceModule(DeviceModule),
      m_PoolModule(PoolModule),
      m_MemoryModule(MemoryModule) {}

luvk::DescriptorSet::~DescriptorSet()
{
    const VkDevice LogicalDevice = m_DeviceModule->GetLogicalDevice();

    if (m_Owned && m_Set != VK_NULL_HANDLE && m_PoolModule)
    {
        vkFreeDescriptorSets(LogicalDevice, m_PoolModule->GetHandle(), 1, &m_Set);
        m_Set = VK_NULL_HANDLE;
    }

    if (m_Layout != VK_NULL_HANDLE && m_OwnsLayout)
    {
        vkDestroyDescriptorSetLayout(LogicalDevice, m_Layout, nullptr);
        m_Layout = VK_NULL_HANDLE;
    }
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

void luvk::DescriptorSet::UseLayout(const VkDescriptorSetLayout Layout)
{
    m_Layout     = Layout;
    m_OwnsLayout = false;
}

void luvk::DescriptorSet::Allocate()
{
    if (m_PoolModule && m_PoolModule->AllocateSets({&m_Layout, 1}, {&m_Set, 1}))
    {
        m_Owned = true;
        return;
    }

    throw std::runtime_error("Failed to allocate descriptor set.");
}

void luvk::DescriptorSet::UpdateBuffer(const VkBuffer         Buffer,
                                       const VkDeviceSize     Size,
                                       const std::uint32_t    Binding,
                                       const VkDescriptorType Type) const
{
    const VkDescriptorBufferInfo BufferInfo{.buffer = Buffer,
                                            .offset = 0,
                                            .range = Size};

    const VkWriteDescriptorSet Write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_Set,
                                     .dstBinding = Binding,
                                     .descriptorCount = 1,
                                     .descriptorType = Type,
                                     .pBufferInfo = &BufferInfo};

    vkUpdateDescriptorSets(m_DeviceModule->GetLogicalDevice(), 1, &Write, 0, nullptr);
}

void luvk::DescriptorSet::UpdateImage(const VkImageView      View,
                                      const VkSampler        Sampler,
                                      const std::uint32_t    Binding,
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
