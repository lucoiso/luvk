// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Buffer.hpp"
#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Upload.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace
{
    VmaAllocationCreateInfo MakeAllocationInfo(const luvk::MemoryUsage Memory, const float Priority)
    {
        VmaAllocationCreateInfo Info{.usage = VMA_MEMORY_USAGE_AUTO, .priority = Priority};

        switch (Memory)
        {
        case luvk::MemoryUsage::GpuOnly:
            Info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            break;
        case luvk::MemoryUsage::Stream:
            // Host-visible device-local memory when the BAR allows it, host memory read over PCIe otherwise.
            Info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            Info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case luvk::MemoryUsage::Upload:
            Info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            Info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        case luvk::MemoryUsage::Readback:
            Info.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
            Info.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
            break;
        }

        return Info;
    }
}

luvk::Buffer::Buffer(Context& Ctx, const BufferDesc& Desc)
    : m_Context(&Ctx),
      m_Size(Desc.Size)
{
    if (Desc.Size == 0U)
    {
        throw std::invalid_argument("[luvk] Buffer size must be greater than zero.");
    }

    const std::span<const std::uint32_t> Families = Ctx.SharedQueueFamilies();

    const VkBufferCreateInfo Info{.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                  .size                  = Desc.Size,
                                  .usage                 = Ctx.DefaultBufferUsage(),
                                  .sharingMode           = std::size(Families) > 1U ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
                                  .queueFamilyIndexCount = std::size(Families) > 1U ? static_cast<std::uint32_t>(std::size(Families)) : 0U,
                                  .pQueueFamilyIndices   = std::size(Families) > 1U ? std::data(Families) : nullptr};

    const VmaAllocationCreateInfo AllocationInfo = MakeAllocationInfo(Desc.Memory, Desc.Priority);

    VmaAllocationInfo Result = {};
    Check(vmaCreateBuffer(Ctx.Allocator(), &Info, &AllocationInfo, &m_Buffer, &m_Allocation, &Result), "vmaCreateBuffer");
    Ctx.TrackResource(+1);

    m_Mapped = static_cast<std::byte*>(Result.pMappedData);

    VkMemoryPropertyFlags Properties = 0U;
    vmaGetAllocationMemoryProperties(Ctx.Allocator(), m_Allocation, &Properties);
    m_Coherent = (Properties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0U;

    const VkBufferDeviceAddressInfo AddressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = m_Buffer};
    m_Address = vkGetBufferDeviceAddress(Ctx.Device(), &AddressInfo);

    if (!std::empty(Desc.Name))
    {
        const std::string Name(Desc.Name);
        vmaSetAllocationName(Ctx.Allocator(), m_Allocation, Name.c_str());
        Ctx.SetDebugName(m_Buffer, VK_OBJECT_TYPE_BUFFER, Name);
    }
}

void luvk::Buffer::NoteTransferUse(const std::uint64_t TransferValue) noexcept
{
    m_TransferUse = TransferValue > m_TransferUse ? TransferValue : m_TransferUse;
}

luvk::Buffer::~Buffer()
{
    Reset();
}

luvk::Buffer::Buffer(Buffer&& Other) noexcept
    : m_Context(std::exchange(Other.m_Context, nullptr)),
      m_Buffer(std::exchange(Other.m_Buffer, VK_NULL_HANDLE)),
      m_Allocation(std::exchange(Other.m_Allocation, nullptr)),
      m_Size(std::exchange(Other.m_Size, 0U)),
      m_Mapped(std::exchange(Other.m_Mapped, nullptr)),
      m_Address(std::exchange(Other.m_Address, 0U)),
      m_Coherent(std::exchange(Other.m_Coherent, false)),
      m_TransferUse(std::exchange(Other.m_TransferUse, 0U)),
      m_State(std::exchange(Other.m_State, {})) {}

luvk::Buffer& luvk::Buffer::operator=(Buffer&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        m_Context    = std::exchange(Other.m_Context, nullptr);
        m_Buffer     = std::exchange(Other.m_Buffer, VK_NULL_HANDLE);
        m_Allocation = std::exchange(Other.m_Allocation, nullptr);
        m_Size       = std::exchange(Other.m_Size, 0U);
        m_Mapped     = std::exchange(Other.m_Mapped, nullptr);
        m_Address    = std::exchange(Other.m_Address, 0U);
        m_Coherent    = std::exchange(Other.m_Coherent, false);
        m_TransferUse = std::exchange(Other.m_TransferUse, 0U);
        m_State       = std::exchange(Other.m_State, {});
    }
    return *this;
}

void luvk::Buffer::Reset() noexcept
{
    if (m_Buffer != VK_NULL_HANDLE)
    {
        TimelinePoint Point = m_Context->CurrentRetirePoint();
        Point.Transfer      = m_TransferUse;
        Release(Point);
    }
}

void luvk::Buffer::Release(const TimelinePoint& Point) noexcept
{
    if (m_Buffer != VK_NULL_HANDLE)
    {
        m_Context->Retire(RetiredObject{.Kind = RetiredKind::Buffer, .Handle = HandleToU64(m_Buffer), .Allocation = m_Allocation}, Point);
        m_Context->TrackResource(-1);
    }

    m_Buffer      = VK_NULL_HANDLE;
    m_Allocation  = nullptr;
    m_Size        = 0U;
    m_Mapped      = nullptr;
    m_Address     = 0U;
    m_TransferUse = 0U;
    m_State       = {};
}

void luvk::Buffer::Write(const std::span<const std::byte> Data, const VkDeviceSize Offset)
{
    CheckRange(Offset, std::size(Data));
    if (std::empty(Data))
    {
        return;
    }

    if (m_Mapped != nullptr)
    {
        std::memcpy(m_Mapped + Offset, std::data(Data), std::size(Data));
        if (!m_Coherent)
        {
            Check(vmaFlushAllocation(m_Context->Allocator(), m_Allocation, Offset, std::size(Data)), "vmaFlushAllocation");
        }
        return;
    }

    m_Context->Uploads().Upload(*this, Data, Offset);
}

void luvk::Buffer::Flush(const VkDeviceSize Offset, const VkDeviceSize Size) const
{
    if (m_Mapped != nullptr && !m_Coherent)
    {
        Check(vmaFlushAllocation(m_Context->Allocator(), m_Allocation, Offset, Size), "vmaFlushAllocation");
    }
}

void luvk::Buffer::Invalidate(const VkDeviceSize Offset, const VkDeviceSize Size) const
{
    if (m_Mapped != nullptr && !m_Coherent)
    {
        Check(vmaInvalidateAllocation(m_Context->Allocator(), m_Allocation, Offset, Size), "vmaInvalidateAllocation");
    }
}

void luvk::Buffer::Read(const std::span<std::byte> Output, const VkDeviceSize Offset) const
{
    CheckRange(Offset, std::size(Output));
    if (m_Mapped == nullptr)
    {
        throw std::logic_error("[luvk] Buffer::Read requires host-visible memory (MemoryUsage::Readback).");
    }

    if (!m_Coherent)
    {
        Check(vmaInvalidateAllocation(m_Context->Allocator(), m_Allocation, Offset, std::size(Output)), "vmaInvalidateAllocation");
    }
    std::memcpy(std::data(Output), m_Mapped + Offset, std::size(Output));
}

void luvk::Buffer::CheckRange(const VkDeviceSize Offset, const VkDeviceSize Size) const
{
    if (m_Buffer == VK_NULL_HANDLE)
    {
        throw std::logic_error("[luvk] Access to an empty Buffer.");
    }

    if (Offset > m_Size || Size > m_Size - Offset)
    {
        throw std::out_of_range("[luvk] Buffer access out of range: offset " + std::to_string(Offset) + " + size " +
                                std::to_string(Size) + " > capacity " + std::to_string(m_Size) + ".");
    }
}
