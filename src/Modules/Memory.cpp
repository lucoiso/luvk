/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Memory.hpp"
#include <stdexcept>
#include "luvk/Interfaces/IServiceLocator.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Instance.hpp"

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

using namespace luvk;

void Memory::OnInitialize(IServiceLocator* ServiceLocator)
{
    m_ServiceLocator = ServiceLocator;
    const auto* DeviceMod = m_ServiceLocator->GetModule<Device>();
    const auto* InstanceMod = m_ServiceLocator->GetModule<Instance>();

    if (!DeviceMod || !InstanceMod)
    {
        throw std::runtime_error("Dependencies missing for Memory module");
    }

    const VmaVulkanFunctions VulkanFunctions{.vkGetInstanceProcAddr = vkGetInstanceProcAddr, .vkGetDeviceProcAddr = vkGetDeviceProcAddr};

    const VmaAllocatorCreateInfo AllocatorInfo{.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
                                               .physicalDevice = DeviceMod->GetPhysicalDevice(),
                                               .device = DeviceMod->GetLogicalDevice(),
                                               .pVulkanFunctions = &VulkanFunctions,
                                               .instance = InstanceMod->GetHandle(),
                                               .vulkanApiVersion = VK_API_VERSION_1_4};

    if (!LUVK_EXECUTE(vmaCreateAllocator(&AllocatorInfo, &m_Allocator)))
    {
        throw std::runtime_error("Failed to create VMA allocator");
    }

    IModule::OnInitialize(ServiceLocator);
}

void Memory::OnShutdown()
{
    if (m_Allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = VK_NULL_HANDLE;
    }

    IModule::OnShutdown();
}
