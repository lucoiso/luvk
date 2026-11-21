// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Bindless.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace
{
    constexpr std::array<VkDescriptorType, 4> DescriptorTypes{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
                                                              VK_DESCRIPTOR_TYPE_SAMPLER,
                                                              VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                              VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR};

    std::uint32_t AccelerationStructureCount(const luvk::Context& Ctx, const std::uint32_t Requested)
    {
        const auto* const Features = Ctx.EnabledFeatures().Find<VkPhysicalDeviceAccelerationStructureFeaturesKHR>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR);

        if (Features == nullptr || Features->accelerationStructure == VK_FALSE ||
            Features->descriptorBindingAccelerationStructureUpdateAfterBind == VK_FALSE ||
            !Ctx.IsDeviceExtensionEnabled(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
        {
            return 0U;
        }

        VkPhysicalDeviceAccelerationStructurePropertiesKHR Properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
        VkPhysicalDeviceProperties2                        Wrapper{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &Properties};
        vkGetPhysicalDeviceProperties2(Ctx.PhysicalDevice(), &Wrapper);

        return std::min({Requested,
                         Properties.maxDescriptorSetUpdateAfterBindAccelerationStructures,
                         Properties.maxPerStageDescriptorUpdateAfterBindAccelerationStructures});
    }

    constexpr std::size_t ToIndex(const luvk::BindlessSlot Slot) noexcept
    {
        return static_cast<std::size_t>(Slot);
    }
}

luvk::BindlessHeap::BindlessHeap(Context& Ctx, const BindlessLimits& Limits)
    : m_Context(Ctx)
{
    const VkPhysicalDeviceVulkan12Properties& Props = Ctx.Properties12();

    std::array<std::uint32_t, 3> Counts{
        std::min({Limits.SampledImages, Props.maxDescriptorSetUpdateAfterBindSampledImages, Props.maxPerStageDescriptorUpdateAfterBindSampledImages}),
        std::min({Limits.Samplers, Props.maxDescriptorSetUpdateAfterBindSamplers, Props.maxPerStageDescriptorUpdateAfterBindSamplers}),
        std::min({Limits.StorageImages, Props.maxDescriptorSetUpdateAfterBindStorageImages, Props.maxPerStageDescriptorUpdateAfterBindStorageImages})};

    const std::uint64_t Total = static_cast<std::uint64_t>(Counts[0]) + Counts[1] + Counts[2];
    if (const std::uint64_t Limit = Props.maxPerStageUpdateAfterBindResources;
        Total > Limit)
    {
        for (std::uint32_t& Count : Counts)
        {
            Count = static_cast<std::uint32_t>(std::max<std::uint64_t>(1U, Count * Limit / Total));
        }
    }

    for (std::size_t Index = 0U; Index < std::size(Counts); ++Index)
    {
        Counts[Index]                 = std::max(1U, Counts[Index]);
        m_Allocators[Index].Capacity = Counts[Index];
    }

    const std::uint32_t StructureCount = AccelerationStructureCount(Ctx, Limits.AccelerationStructures);
    m_Allocators[ToIndex(BindlessSlot::AccelerationStructure)].Capacity = StructureCount;

    std::vector<VkDescriptorSetLayoutBinding> Bindings;
    std::vector<VkDescriptorBindingFlags>     BindingFlags;
    std::vector<VkDescriptorPoolSize>         PoolSizes;

    for (std::uint32_t Index = 0U; Index < 4U; ++Index)
    {
        const std::uint32_t Count = Index < 3U ? Counts[Index] : StructureCount;
        if (Count == 0U)
        {
            continue;
        }

        Bindings.push_back(VkDescriptorSetLayoutBinding{.binding         = Index,
                                                        .descriptorType  = DescriptorTypes[Index],
                                                        .descriptorCount = Count,
                                                        .stageFlags      = VK_SHADER_STAGE_ALL});
        BindingFlags.push_back(VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                               VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
                               VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT);
        PoolSizes.push_back(VkDescriptorPoolSize{.type = DescriptorTypes[Index], .descriptorCount = Count});
    }

    const VkDescriptorSetLayoutBindingFlagsCreateInfo FlagsInfo{.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
                                                                .bindingCount  = static_cast<std::uint32_t>(std::size(BindingFlags)),
                                                                .pBindingFlags = std::data(BindingFlags)};

    const VkDescriptorSetLayoutCreateInfo LayoutInfo{.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                                                     .pNext        = &FlagsInfo,
                                                     .flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                     .bindingCount = static_cast<std::uint32_t>(std::size(Bindings)),
                                                     .pBindings    = std::data(Bindings)};

    const VkDevice Device = Ctx.Device();

    try
    {
        Check(vkCreateDescriptorSetLayout(Device, &LayoutInfo, nullptr, &m_SetLayout), "vkCreateDescriptorSetLayout (bindless)");

        const VkDescriptorPoolCreateInfo PoolInfo{.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                                  .flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
                                                  .maxSets       = 1U,
                                                  .poolSizeCount = static_cast<std::uint32_t>(std::size(PoolSizes)),
                                                  .pPoolSizes    = std::data(PoolSizes)};
        Check(vkCreateDescriptorPool(Device, &PoolInfo, nullptr, &m_Pool), "vkCreateDescriptorPool (bindless)");

        const VkDescriptorSetAllocateInfo AllocateInfo{.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                                       .descriptorPool     = m_Pool,
                                                       .descriptorSetCount = 1U,
                                                       .pSetLayouts        = &m_SetLayout};
        Check(vkAllocateDescriptorSets(Device, &AllocateInfo, &m_Set), "vkAllocateDescriptorSets (bindless)");

        // PushConstantSize is the spec minimum of maxPushConstantsSize, so the whole block is always available and
        // CommandList::PushConstants can validate against the same constant.
        if (Ctx.Properties().limits.maxPushConstantsSize < PushConstantSize)
        {
            throw std::runtime_error("[luvk] The device reports maxPushConstantsSize below the 128 bytes required by the specification.");
        }

        const VkPushConstantRange PushRange{.stageFlags = VK_SHADER_STAGE_ALL, .offset = 0U, .size = PushConstantSize};

        const VkPipelineLayoutCreateInfo PipelineLayoutInfo{.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                            .setLayoutCount         = 1U,
                                                            .pSetLayouts            = &m_SetLayout,
                                                            .pushConstantRangeCount = 1U,
                                                            .pPushConstantRanges    = &PushRange};
        Check(vkCreatePipelineLayout(Device, &PipelineLayoutInfo, nullptr, &m_PipelineLayout), "vkCreatePipelineLayout (bindless)");
    }
    catch (...)
    {
        Release();
        throw;
    }

    Ctx.SetDebugName(m_SetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, "luvk bindless layout");
    Ctx.SetDebugName(m_Set, VK_OBJECT_TYPE_DESCRIPTOR_SET, "luvk bindless set");
    Ctx.SetDebugName(m_PipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, "luvk pipeline layout");
}

luvk::BindlessHeap::~BindlessHeap()
{
    Release();
}

void luvk::BindlessHeap::Release() noexcept
{
    const VkDevice Device = m_Context.Device();

    if (m_PipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(Device, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }

    if (m_Pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(Device, m_Pool, nullptr);
        m_Pool = VK_NULL_HANDLE;
        m_Set  = VK_NULL_HANDLE;
    }

    if (m_SetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(Device, m_SetLayout, nullptr);
        m_SetLayout = VK_NULL_HANDLE;
    }
}

std::uint32_t luvk::BindlessHeap::Allocate(const BindlessSlot Slot)
{
    std::scoped_lock Lock(m_Mutex);
    IndexAllocator&  Allocator = m_Allocators[ToIndex(Slot)];

    if (!std::empty(Allocator.FreeList))
    {
        const std::uint32_t Index = Allocator.FreeList.back();
        Allocator.FreeList.pop_back();
        return Index;
    }

    if (Allocator.Next >= Allocator.Capacity)
    {
        throw std::runtime_error("[luvk] Bindless heap is full (slot " + std::to_string(ToIndex(Slot)) + ", capacity " +
                                 std::to_string(Allocator.Capacity) + ").");
    }

    return Allocator.Next++;
}

void luvk::BindlessHeap::Free(const BindlessSlot Slot, const std::uint32_t Index) noexcept
{
    if (Index == InvalidBindlessIndex)
    {
        return;
    }

    std::scoped_lock Lock(m_Mutex);
    IndexAllocator&  Allocator = m_Allocators[ToIndex(Slot)];

    // A double or out-of-range free would hand the same descriptor slot to two resources later on.
    if (Index >= Allocator.Next || std::ranges::find(Allocator.FreeList, Index) != std::end(Allocator.FreeList))
    {
        return;
    }
    Allocator.FreeList.push_back(Index);
}

void luvk::BindlessHeap::WriteSampledImage(const std::uint32_t Index, const VkImageView View) const
{
    Write(BindlessSlot::SampledImage, Index, VkDescriptorImageInfo{.imageView = View, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
}

void luvk::BindlessHeap::WriteSampler(const std::uint32_t Index, const VkSampler Sampler) const
{
    Write(BindlessSlot::Sampler, Index, VkDescriptorImageInfo{.sampler = Sampler});
}

void luvk::BindlessHeap::WriteStorageImage(const std::uint32_t Index, const VkImageView View) const
{
    Write(BindlessSlot::StorageImage, Index, VkDescriptorImageInfo{.imageView = View, .imageLayout = VK_IMAGE_LAYOUT_GENERAL});
}

void luvk::BindlessHeap::WriteAccelerationStructure(const std::uint32_t Index, const VkAccelerationStructureKHR Structure) const
{
    if (Index >= Capacity(BindlessSlot::AccelerationStructure))
    {
        throw std::out_of_range("[luvk] Bindless acceleration structure index out of range (is the RayTracingModule enabled?).");
    }

    const VkWriteDescriptorSetAccelerationStructureKHR StructureInfo{.sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                                                                     .accelerationStructureCount = 1U,
                                                                     .pAccelerationStructures    = &Structure};

    const VkWriteDescriptorSet Write{.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .pNext           = &StructureInfo,
                                     .dstSet          = m_Set,
                                     .dstBinding      = static_cast<std::uint32_t>(BindlessSlot::AccelerationStructure),
                                     .dstArrayElement = Index,
                                     .descriptorCount = 1U,
                                     .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR};

    std::scoped_lock Lock(m_Mutex);
    vkUpdateDescriptorSets(m_Context.Device(), 1U, &Write, 0U, nullptr);
}

std::uint32_t luvk::BindlessHeap::Capacity(const BindlessSlot Slot) const noexcept
{
    return m_Allocators[ToIndex(Slot)].Capacity;
}

std::uint32_t luvk::BindlessHeap::InUse(const BindlessSlot Slot) const noexcept
{
    std::scoped_lock      Lock(m_Mutex);
    const IndexAllocator& Allocator = m_Allocators[ToIndex(Slot)];
    return Allocator.Next - static_cast<std::uint32_t>(std::size(Allocator.FreeList));
}

void luvk::BindlessHeap::Write(const BindlessSlot Slot, const std::uint32_t Index, const VkDescriptorImageInfo& Info) const
{
    if (Index >= Capacity(Slot))
    {
        throw std::out_of_range("[luvk] Bindless index out of range.");
    }

    const VkWriteDescriptorSet Write{.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                                     .dstSet          = m_Set,
                                     .dstBinding      = static_cast<std::uint32_t>(Slot),
                                     .dstArrayElement = Index,
                                     .descriptorCount = 1U,
                                     .descriptorType  = DescriptorTypes[ToIndex(Slot)],
                                     .pImageInfo      = &Info};

    std::scoped_lock Lock(m_Mutex);
    vkUpdateDescriptorSets(m_Context.Device(), 1U, &Write, 0U, nullptr);
}
