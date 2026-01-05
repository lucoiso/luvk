/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/DescriptorPool.hpp"
#include <array>
#include "luvk/Interfaces/IServiceLocator.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

void DescriptorPool::OnInitialize(IServiceLocator* ServiceLocator)
{
    m_ServiceLocator = ServiceLocator;
    IModule::OnInitialize(ServiceLocator);
}

void DescriptorPool::OnShutdown()
{
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    const VkDevice Device = DeviceMod->GetLogicalDevice();

    for (const auto Pool : m_UsedPools)
    {
        vkDestroyDescriptorPool(Device, Pool, nullptr);
    }

    for (const auto Pool : m_FreePools)
    {
        vkDestroyDescriptorPool(Device, Pool, nullptr);
    }

    m_UsedPools.clear();
    m_FreePools.clear();
    m_CurrentPool = VK_NULL_HANDLE;

    IModule::OnShutdown();
}

VkDescriptorPool DescriptorPool::CreatePool() const
{
    static constexpr std::array<VkDescriptorPoolSize, 11> Sizes{{{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                                                 {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}}};

    constexpr VkDescriptorPoolCreateInfo Info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                              .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
                                              .maxSets = 1000,
                                              .poolSizeCount = static_cast<std::uint32_t>(std::size(Sizes)),
                                              .pPoolSizes = std::data(Sizes)};

    VkDescriptorPool Pool;
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    vkCreateDescriptorPool(DeviceMod->GetLogicalDevice(), &Info, nullptr, &Pool);
    return Pool;
}

VkDescriptorPool DescriptorPool::GetPool()
{
    if (!m_FreePools.empty())
    {
        const VkDescriptorPool Pool = m_FreePools.back();
        m_FreePools.pop_back();
        return Pool;
    }
    return CreatePool();
}

bool DescriptorPool::AllocateSet(VkDescriptorSetLayout Layout, VkDescriptorSet& Set)
{
    std::lock_guard Lock(m_Mutex);

    if (m_CurrentPool == VK_NULL_HANDLE)
    {
        m_CurrentPool = GetPool();
        m_UsedPools.push_back(m_CurrentPool);
    }

    VkDescriptorSetAllocateInfo Info{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                     .descriptorPool = m_CurrentPool,
                                     .descriptorSetCount = 1,
                                     .pSetLayouts = &Layout};

    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    VkResult Result = vkAllocateDescriptorSets(DeviceMod->GetLogicalDevice(), &Info, &Set);

    if (Result == VK_ERROR_OUT_OF_POOL_MEMORY || Result == VK_ERROR_FRAGMENTED_POOL)
    {
        m_CurrentPool = GetPool();
        m_UsedPools.push_back(m_CurrentPool);
        Info.descriptorPool = m_CurrentPool;
        Result = vkAllocateDescriptorSets(DeviceMod->GetLogicalDevice(), &Info, &Set);
    }

    return Result == VK_SUCCESS;
}

void DescriptorPool::Reset()
{
    std::lock_guard Lock(m_Mutex);
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();

    for (auto Pool : m_UsedPools)
    {
        vkResetDescriptorPool(DeviceMod->GetLogicalDevice(), Pool, 0);
        m_FreePools.push_back(Pool);
    }
    m_UsedPools.clear();
    m_CurrentPool = VK_NULL_HANDLE;
}
