/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Buffer.hpp"
#include <stdexcept>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Modules/Memory.hpp"

using namespace luvk;

Buffer::Buffer(Device* DeviceModule, Memory* MemoryModule)
    : m_DeviceModule(DeviceModule),
      m_MemoryModule(MemoryModule) {}

Buffer::~Buffer()
{
    if (m_Buffer != VK_NULL_HANDLE)
    {
        const VmaAllocator Allocator = m_MemoryModule->GetAllocator();
        if (m_Map != nullptr && !m_IsPersistentMap)
        {
            vmaUnmapMemory(Allocator, m_Allocation);
        }
        vmaDestroyBuffer(Allocator, m_Buffer, m_Allocation);
    }
}

void Buffer::CreateBuffer(const BufferCreationArguments& Arguments)
{
    if (m_Buffer != VK_NULL_HANDLE)
    {
        const VmaAllocator Allocator = m_MemoryModule->GetAllocator();
        if (m_Map != nullptr && !m_IsPersistentMap)
        {
            vmaUnmapMemory(Allocator, m_Allocation);
        }
        vmaDestroyBuffer(Allocator, m_Buffer, m_Allocation);
        m_Buffer = VK_NULL_HANDLE;
        m_Allocation = nullptr;
        m_Map = nullptr;
        m_DeviceAddress = 0U;
    }

    m_Size = Arguments.Size;
    m_IsPersistentMap = Arguments.PersistentlyMapped;

    const VkBufferUsageFlags UsageFlags = Arguments.Usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    const VkBufferCreateInfo Info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = m_Size, .usage = UsageFlags, .sharingMode = VK_SHARING_MODE_EXCLUSIVE};

    VmaAllocationCreateFlags AllocFlags = 0;
    if (m_IsPersistentMap)
    {
        AllocFlags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    if (Arguments.MemoryUsage == VMA_MEMORY_USAGE_CPU_TO_GPU || Arguments.MemoryUsage == VMA_MEMORY_USAGE_CPU_ONLY)
    {
        AllocFlags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    const VmaAllocationCreateInfo AllocInfo{.flags = AllocFlags, .usage = Arguments.MemoryUsage, .priority = Arguments.Priority};

    if (!LUVK_EXECUTE(vmaCreateBuffer(m_MemoryModule->GetAllocator(), &Info, &AllocInfo, &m_Buffer, &m_Allocation, nullptr)))
    {
        throw std::runtime_error("Failed to create buffer");
    }

    if (m_IsPersistentMap)
    {
        VmaAllocationInfo ResultInfo;
        vmaGetAllocationInfo(m_MemoryModule->GetAllocator(), m_Allocation, &ResultInfo);
        m_Map = ResultInfo.pMappedData;
    }

    if (!std::empty(Arguments.Name))
    {
        vmaSetAllocationName(m_MemoryModule->GetAllocator(), m_Allocation, std::data(Arguments.Name));
    }

    const VkBufferDeviceAddressInfo AddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = m_Buffer};
    m_DeviceAddress = vkGetBufferDeviceAddress(m_DeviceModule->GetLogicalDevice(), &AddressInfo);
}

void Buffer::Upload(const std::span<const std::byte> Data, const std::uint64_t Offset) const
{
    if (std::empty(Data))
    {
        return;
    }

    const VmaAllocator Allocator = m_MemoryModule->GetAllocator();

    if (m_Map != nullptr)
    {
        std::memcpy(static_cast<std::byte*>(m_Map) + Offset, std::data(Data), std::size(Data));
        return;
    }

    VmaAllocationInfo AllocationInfo;
    vmaGetAllocationInfo(Allocator, m_Allocation, &AllocationInfo);
    VkMemoryPropertyFlags MemFlags;
    vmaGetMemoryTypeProperties(Allocator, AllocationInfo.memoryType, &MemFlags);

    if (MemFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        void* TempMap = nullptr;
        if (vmaMapMemory(Allocator, m_Allocation, &TempMap) == VK_SUCCESS)
        {
            std::memcpy(static_cast<std::byte*>(TempMap) + Offset, std::data(Data), std::size(Data));
            vmaUnmapMemory(Allocator, m_Allocation);
            return;
        }
    }

    const auto Staging = std::make_unique<Buffer>(m_DeviceModule, m_MemoryModule);
    Staging->CreateBuffer({.Size = std::size(Data),
                           .Usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                           .MemoryUsage = VMA_MEMORY_USAGE_CPU_ONLY,
                           .PersistentlyMapped = true,
                           .Name = "StagingBuffer"});

    Staging->Upload(Data);

    m_DeviceModule->SubmitImmediate([&](const VkCommandBuffer Cmd)
    {
        const VkBufferCopy Region{.srcOffset = 0U, .dstOffset = Offset, .size = std::size(Data)};
        vkCmdCopyBuffer(Cmd, Staging->GetHandle(), m_Buffer, 1, &Region);
    });
}
