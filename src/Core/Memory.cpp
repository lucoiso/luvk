// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Memory.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Device.hpp"

#ifndef VMA_IMPLEMENTATION
    #define VMA_IMPLEMENTATION
#endif
#include <vma/vk_mem_alloc.h>

void luvk::Memory::InitializeAllocator(std::shared_ptr<IRenderModule> const& MainRenderer,
                                       std::shared_ptr<IRenderModule> const& DeviceModule,
                                       VmaAllocatorCreateFlags const Flags)
{
    auto const CastRendererModule = static_cast<luvk::Renderer*>(MainRenderer.get());
    auto const CastDeviceModule = static_cast<luvk::Device*>(DeviceModule.get());

    VmaVulkanFunctions const VulkanFunctions{.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                                             .vkGetDeviceProcAddr = vkGetDeviceProcAddr};

    VmaAllocatorCreateInfo const AllocatorInfo {.flags = Flags,
                                                .physicalDevice = CastDeviceModule->GetPhysicalDevice(),
                                                .device = CastDeviceModule->GetLogicalDevice(),
                                                .preferredLargeHeapBlockSize = 0U /*Default: 256 MiB*/,
                                                .pAllocationCallbacks = nullptr,
                                                .pDeviceMemoryCallbacks = nullptr,
                                                .pHeapSizeLimit = nullptr,
                                                .pVulkanFunctions = &VulkanFunctions,
                                                .instance = CastRendererModule->GetInstance(),
                                                .vulkanApiVersion = VK_API_VERSION_1_3,
                                                .pTypeExternalMemoryHandleTypes = nullptr};

    if (vmaCreateAllocator(&AllocatorInfo, &m_Allocator) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to initialize the allocator.");
    }
}

void luvk::Memory::InitializeDependencies(std::shared_ptr<IRenderModule> const &MainRenderer)
{
    // Do nothing: Needs physical & logical devices to be ready
}

void luvk::Memory::ClearResources(IRenderModule *MainRenderer)
{
    if (m_Allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = VK_NULL_HANDLE;
    }
}
