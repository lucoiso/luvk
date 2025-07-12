// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Buffer.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Memory.hpp"
#include "luvk/Core/Device.hpp"

#include "luvk/Libraries/VulkanHelpers.hpp"
#include <iterator>

void luvk::Buffer::CreateBuffer(std::shared_ptr<Memory> const& MemoryModule, CreationArguments const& Arguments)
{
    m_MemoryModule = MemoryModule;
    auto const* Memory = MemoryModule.get();
    VmaAllocator const& Allocator = Memory->GetAllocator();

    m_Size = Arguments.Size;
    VkBufferCreateInfo const Info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .pNext = nullptr,
                                  .flags = 0,
                                  .size = Arguments.Size,
                                  .usage = Arguments.Usage,
                                  .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

    VmaAllocationCreateInfo const AllocInfo{.flags = Arguments.MemoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU
                                                         ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                         : 0U,
                                            .usage = Arguments.MemoryUsage};

    if (!LUVK_EXECUTE(vmaCreateBuffer(Allocator, &Info, &AllocInfo, &m_Buffer, &m_Allocation, nullptr)))
    {
        throw std::runtime_error("Failed to create buffer.");
    }
}

void luvk::Buffer::RecreateBuffer(CreationArguments const& Arguments)
{
    auto const* Memory = m_MemoryModule.get();
    VmaAllocator const& Allocator = Memory->GetAllocator();
    auto const DeviceModule = Memory->GetDeviceModule();
    const auto* Device = DeviceModule
                             ? dynamic_cast<luvk::Device*>(DeviceModule.get())
                             : nullptr;

    if (Device)
    {
        Device->WaitIdle();
    }

    if (m_Buffer != VK_NULL_HANDLE)
    {
        vmaDestroyBuffer(Allocator, m_Buffer, m_Allocation);
        m_Buffer = VK_NULL_HANDLE;
        m_Allocation = nullptr;
    }

    CreateBuffer(m_MemoryModule, Arguments);
}

void luvk::Buffer::Upload(const std::span<const std::byte> Data) const
{
    auto const* Memory = m_MemoryModule.get();
    VmaAllocator const& Allocator = Memory->GetAllocator();

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
        auto const* Memory = m_MemoryModule.get();
        VmaAllocator const& Allocator = Memory->GetAllocator();
        if (m_Buffer != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(Allocator, m_Buffer, m_Allocation);
        }
    }
}
