// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Buffer.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"
#include "luvk/Modules/Renderer.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"

#include <cstring>
#include <iterator>

void luvk::Buffer::CreateBuffer(const std::shared_ptr<Memory>& MemoryModule, const CreationArguments& Arguments)
{
    m_MemoryModule = MemoryModule;
    m_DeviceModule = std::dynamic_pointer_cast<Device>(m_MemoryModule->GetDeviceModule());

    const VmaAllocator& Allocator = m_MemoryModule->GetAllocator();

    m_Size = Arguments.Size;
    const VkBufferCreateInfo Info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .pNext = nullptr,
                                  .flags = 0,
                                  .size = Arguments.Size,
                                  .usage = Arguments.Usage,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

    const VmaAllocationCreateInfo AllocInfo{.flags = Arguments.MemoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU
                                                         ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                         : 0U,
                                            .usage = Arguments.MemoryUsage,
                                            .priority = Arguments.Priority};

    if (!LUVK_EXECUTE(vmaCreateBuffer(Allocator, &Info, &AllocInfo, &m_Buffer, &m_Allocation, nullptr)))
    {
        throw std::runtime_error("Failed to create buffer.");
    }
}

void luvk::Buffer::RecreateBuffer(const CreationArguments& Arguments)
{
    const VmaAllocator& Allocator = m_MemoryModule->GetAllocator();
    m_DeviceModule->WaitIdle();

    if (m_Buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(Allocator, m_Buffer, m_Allocation);
        m_Buffer = VK_NULL_HANDLE;
        m_Allocation = nullptr;
    }

    CreateBuffer(m_MemoryModule, Arguments);
}

void luvk::Buffer::Upload(const std::span<const std::byte>& Data) const
{
    const VmaAllocator& Allocator = m_MemoryModule->GetAllocator();

    void* Mapping = nullptr;
    if (!LUVK_EXECUTE(vmaMapMemory(Allocator, m_Allocation, &Mapping)))
    {
        throw std::runtime_error("Failed to map buffer memory.");
    }

    if (std::size(Data) > m_Size)
    {
        vmaUnmapMemory(Allocator, m_Allocation);
        throw std::runtime_error("Upload size exceeds buffer capacity.");
    }

    std::memcpy(Mapping, std::data(Data), std::size(Data));
    vmaFlushAllocation(Allocator, m_Allocation, 0, std::size(Data));
    vmaUnmapMemory(Allocator, m_Allocation);
}

luvk::Buffer::~Buffer()
{
    if (m_MemoryModule)
    {
        const VmaAllocator& Allocator = m_MemoryModule->GetAllocator();
        if (m_Buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(Allocator, m_Buffer, m_Allocation);
        }
    }
}
