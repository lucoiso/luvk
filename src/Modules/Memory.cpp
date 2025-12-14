// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Memory.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Renderer.hpp"

#ifndef VMA_IMPLEMENTATION
#    define VMA_LEAK_LOG_FORMAT(format, ...)                                            \
        do                                                                              \
        {                                                                               \
            std::size_t Size = std::snprintf(nullptr, 0, format, __VA_ARGS__);          \
            std::string Message(Size + 1, '\0');                                        \
            std::snprintf(std::data(Message), std::size(Message), format, __VA_ARGS__); \
            Message.pop_back();                                                         \
            std::fprintf(stderr, "%s\n", Message.c_str());                              \
        } while (false)
#    define VMA_IMPLEMENTATION
#endif
#include <vma/vk_mem_alloc.h>

luvk::Memory::Memory(const std::shared_ptr<Renderer>& RendererModule, const std::shared_ptr<Device>& DeviceModule)
    : m_DeviceModule(DeviceModule),
      m_RendererModule(RendererModule) {}

void luvk::Memory::InitializeAllocator(const VmaAllocatorCreateFlags Flags)
{
    VkPhysicalDeviceMemoryProperties MemProps{};
    vkGetPhysicalDeviceMemoryProperties(m_DeviceModule->GetPhysicalDevice(), &MemProps);

    const VmaVulkanFunctions VulkanFunctions{.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
                                             .vkGetDeviceProcAddr = vkGetDeviceProcAddr};

    const VmaAllocatorCreateInfo AllocatorInfo{.flags = Flags | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT,
                                               .physicalDevice = m_DeviceModule->GetPhysicalDevice(),
                                               .device = m_DeviceModule->GetLogicalDevice(),
                                               .preferredLargeHeapBlockSize = 0U,
                                               .pAllocationCallbacks = nullptr,
                                               .pDeviceMemoryCallbacks = nullptr,
                                               .pHeapSizeLimit = nullptr,
                                               .pVulkanFunctions = &VulkanFunctions,
                                               .instance = m_RendererModule->GetInstance(),
                                               .vulkanApiVersion = VK_MAKE_API_VERSION(0, 1, 0, 0),
                                               .pTypeExternalMemoryHandleTypes = nullptr};

    if (!LUVK_EXECUTE(vmaCreateAllocator(&AllocatorInfo, &m_Allocator)))
    {
        throw std::runtime_error("Failed to initialize the allocator.");
    }

    GetEventSystem().Execute(MemoryEvents::OnAllocatorCreated);
}

void luvk::Memory::ClearResources()
{
    if (m_Allocator != VK_NULL_HANDLE)
    {
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = VK_NULL_HANDLE;

        GetEventSystem().Execute(MemoryEvents::OnAllocatorDestroyed);
    }
}
