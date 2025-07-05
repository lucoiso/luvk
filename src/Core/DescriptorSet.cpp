// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/DescriptorSet.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Device.hpp"
#include "luvk/Core/DescriptorPool.hpp"
#include "luvk/Core/Memory.hpp"
#include <vector>
#include "luvk/Libraries/VulkanHelpers.hpp"

void luvk::DescriptorSet::CreateLayout(std::shared_ptr<Device> const& DeviceModule, LayoutInfo const& Info)
{
    m_DeviceModule = DeviceModule;
    m_OwnsLayout = true;
    auto const* Device = DeviceModule.get();
    VkDevice const& LogicalDevice = Device->GetLogicalDevice();

    VkDescriptorSetLayoutCreateInfo const CreateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                     .pNext = nullptr,
                                                     .flags = 0,
                                                     .bindingCount = static_cast<std::uint32_t>(std::size(Info.Bindings)),
                                                     .pBindings = std::data(Info.Bindings)};

    if (!LUVK_EXECUTE(vkCreateDescriptorSetLayout(LogicalDevice, &CreateInfo, nullptr, &m_Layout)))
    {
        throw std::runtime_error("Failed to create descriptor set layout.");
    }
}

void luvk::DescriptorSet::UseLayout(std::shared_ptr<Device> const& DeviceModule, const VkDescriptorSetLayout Layout)
{
    m_DeviceModule = DeviceModule;
    m_Layout = Layout;
    m_OwnsLayout = false;
}

void luvk::DescriptorSet::Allocate(std::shared_ptr<Device> const& DeviceModule,
                                   std::shared_ptr<DescriptorPool> const& PoolModule,
                                   std::shared_ptr<Memory> const& MemoryModule)
{
    m_DeviceModule = DeviceModule;
    m_PoolModule = PoolModule;
    m_MemoryModule = MemoryModule;
    auto const* Device = DeviceModule.get();
    auto const* Pool = PoolModule.get();

    VkDevice const& LogicalDevice = Device->GetLogicalDevice();
    const VkDescriptorPool pool = Pool->GetHandle();

    VkDescriptorSetAllocateInfo const AllocateInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                   .descriptorPool = pool,
                                                   .descriptorSetCount = 1,
                                                   .pSetLayouts = &m_Layout};

    if (!LUVK_EXECUTE(vkAllocateDescriptorSets(LogicalDevice, &AllocateInfo, &m_Set)))
    {
        throw std::runtime_error("Failed to allocate descriptor set.");
    }
}

void luvk::DescriptorSet::UpdateUniform(std::shared_ptr<Device> const& DeviceModule, VkBuffer const Buffer, VkDeviceSize const Size) const
{
    auto const* Device = DeviceModule.get();
    VkDevice const& LogicalDevice = Device->GetLogicalDevice();

    VkDescriptorBufferInfo const BufferInfo{.buffer = Buffer, .offset = 0, .range = Size};

    VkWriteDescriptorSet const Write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_Set,
                                     .dstBinding = 0,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                     .pBufferInfo = &BufferInfo};

    vkUpdateDescriptorSets(LogicalDevice, 1, &Write, 0, nullptr);
}

void luvk::DescriptorSet::UpdateImage(std::shared_ptr<Device> const& DeviceModule, VkImageView const View, VkSampler const Sampler) const
{
    auto const* Device = DeviceModule.get();
    VkDevice const& LogicalDevice = Device->GetLogicalDevice();

    VkDescriptorImageInfo const ImageInfo{.sampler = Sampler, .imageView = View, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

    VkWriteDescriptorSet const Write{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet = m_Set,
                                     .dstBinding = 0,
                                     .descriptorCount = 1,
                                     .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                     .pImageInfo = &ImageInfo};

    vkUpdateDescriptorSets(LogicalDevice, 1, &Write, 0, nullptr);
}

luvk::DescriptorSet::~DescriptorSet()
{
    Destroy();
}

void luvk::DescriptorSet::Destroy()
{
    if (m_DeviceModule)
    {
        const VkDevice LogicalDevice = m_DeviceModule->GetLogicalDevice();
        if (m_Set != VK_NULL_HANDLE && m_PoolModule)
        {
            auto const* Pool = m_PoolModule.get();
            vkFreeDescriptorSets(LogicalDevice, Pool->GetHandle(), 1, &m_Set);
            m_Set = VK_NULL_HANDLE;
        }
        if (m_Layout != VK_NULL_HANDLE && m_OwnsLayout)
        {
            vkDestroyDescriptorSetLayout(LogicalDevice, m_Layout, nullptr);
            m_Layout = VK_NULL_HANDLE;
        }
        m_MemoryModule.reset();
    }
}
