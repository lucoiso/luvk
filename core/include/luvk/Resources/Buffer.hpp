// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <type_traits>
#include "luvk/Core/DeletionQueue.hpp"
#include "luvk/Core/Common.hpp"
#include "luvk/Core/Vma.hpp"
#include "luvk/Resources/ResourceState.hpp"

namespace luvk
{
    class Context;

    /** Memory placement policy for buffer allocations. */
    enum class MemoryUsage : std::uint8_t
    {
        /** Device-local; Write() stages the data through the transfer queue. */
        GpuOnly,

        /** Always mapped, written by the CPU and read by the GPU, preferably device-local (ReBAR). Writes are direct, so
         *  only write ranges the GPU is not reading: per-frame data belongs in a FrameAllocator. */
        Stream,

        /** Host memory for staging copies. */
        Upload,

        /** Host memory the GPU writes and the CPU reads back. */
        Readback,
    };

    /** Ordering of a staged buffer upload relative to the graphics work submitted before it. */
    enum class UploadOrder : std::uint8_t
    {
        /** The transfer batch waits for every graphics submission made before it: overwriting data that frames in
         *  flight still read is safe. This serialises the GPU between frames while uploads are pending. */
        AfterGraphics,

        /** No wait on graphics work: the caller guarantees that no submitted GPU work reads or writes the destination
         *  range (e.g. a free slot of a pool). Streaming then overlaps rendering. The next frame still waits for the copy. */
        Immediate,
    };

    /** Creation parameters for luvk::Buffer. */
    struct BufferDesc
    {
        /** Buffer size in bytes (must be greater than zero). */
        VkDeviceSize Size = 0U;

        /** VkBufferUsageFlags: TRANSFER_SRC, TRANSFER_DST and SHADER_DEVICE_ADDRESS are always added. */
        VkBufferUsageFlags Usage = 0U;

        /** Memory placement policy. */
        MemoryUsage Memory = MemoryUsage::GpuOnly;

        /** VMA allocation priority in [0, 1] (higher means more likely to get dedicated memory). */
        float Priority = 0.5F;

        /** Optional debug name applied to the allocation and the VkBuffer. */
        std::string_view Name;
    };

    /** Move-only RAII buffer; transfer usage and a device address are always available.
     *  On destruction the VkBuffer is retired to the Context deletion queue, never destroyed while in use. */
    class LUVK_API Buffer
    {
        /** Owning Context used for allocation, uploads and retirement. */
        Context* m_Context = nullptr;

        /** Vulkan buffer handle. */
        VkBuffer m_Buffer = VK_NULL_HANDLE;

        /** VMA allocation backing the buffer. */
        VmaAllocation m_Allocation = nullptr;

        /** Buffer size in bytes. */
        VkDeviceSize m_Size = 0U;

        /** Persistent mapping of host-visible memory, or nullptr for device-local allocations. */
        std::byte* m_Mapped = nullptr;

        /** Shader device address of the buffer start. */
        VkDeviceAddress m_Address = 0U;

        /** True when the memory is host coherent (Write and Read then skip explicit flush or invalidate). */
        bool m_Coherent = false;

        /** Highest transfer-queue timeline value using this buffer; destruction also waits for it. */
        std::uint64_t m_TransferUse = 0U;

        /** Graphics-timeline synchronisation state, maintained by the render graph. */
        ResourceState m_State = {};

    public:
        /** Default constructor. */
        Buffer() = default;

        /** Constructor. Creates the buffer through VMA: Desc.Size must be greater than zero. */
        Buffer(Context& Ctx, const BufferDesc& Desc);

        /** Destructor. */
        ~Buffer();

        /** Non-copyable. */
        Buffer(const Buffer&)            = delete;

        /** Non-copyable. */
        Buffer& operator=(const Buffer&) = delete;

        /** Move-only. */
        Buffer(Buffer&& Other) noexcept;

        /** Move-only. */
        Buffer& operator=(Buffer&& Other) noexcept;

        /** Releases the buffer (deferred) and leaves this object empty. */
        void Reset() noexcept;

        /** Releases the buffer once Point is reached on both timelines (e.g. transfer-only staging buffers). */
        void Release(const TimelinePoint& Point) noexcept;

        /** Records that a transfer batch (timeline value) uses this buffer; destruction then also waits for it. */
        void NoteTransferUse(std::uint64_t TransferValue) noexcept;

        /** Bounds-checked write: mapped memory is written directly (and flushed), otherwise the data is staged and copied on
         *  the transfer queue, ordered as Order says (ignored for mapped memory). */
        void Write(std::span<const std::byte> Data, VkDeviceSize Offset = 0U, UploadOrder Order = UploadOrder::AfterGraphics);

        template <typename T>
            requires std::is_trivially_copyable_v<T>

        /** Writes the trivially copyable elements of Data as raw bytes at the given byte offset. */
        void Write(const std::span<const T> Data, const VkDeviceSize Offset = 0U, const UploadOrder Order = UploadOrder::AfterGraphics)
        {
            Write(std::as_bytes(Data), Offset, Order);
        }

        template <typename T>
            requires std::is_trivially_copyable_v<T>

        /** Writes a single trivially copyable Value at the given byte offset. */
        void WriteValue(const T& Value, const VkDeviceSize Offset = 0U, const UploadOrder Order = UploadOrder::AfterGraphics)
        {
            Write(std::as_bytes(std::span<const T, 1>(&Value, 1U)), Offset, Order);
        }

        /** Makes CPU writes done through Mapped() visible to the GPU (no-op for host-coherent memory). */
        void Flush(VkDeviceSize Offset = 0U, VkDeviceSize Size = VK_WHOLE_SIZE) const;

        /** Bounds-checked read of host-visible memory (invalidates non-coherent memory first). */
        void Read(std::span<std::byte> Output, VkDeviceSize Offset = 0U) const;

        /** True when a VkBuffer was created. */
        [[nodiscard]] explicit operator bool() const noexcept { return m_Buffer != VK_NULL_HANDLE; }

        /** Underlying VkBuffer handle. */
        [[nodiscard]] VkBuffer        Handle() const noexcept { return m_Buffer; }

        /** Buffer size in bytes. */
        [[nodiscard]] VkDeviceSize    Size() const noexcept { return m_Size; }

        /** Shader device address of the buffer start. */
        [[nodiscard]] VkDeviceAddress Address() const noexcept { return m_Address; }

        /** True when the buffer is host visible and persistently mapped. */
        [[nodiscard]] bool            IsMapped() const noexcept { return m_Mapped != nullptr; }

        /** Underlying VMA allocation. */
        [[nodiscard]] VmaAllocation   Allocation() const noexcept { return m_Allocation; }

        /** Mapped bytes (empty when not host visible). Writing here bypasses bounds/flush handling of Write(). */
        [[nodiscard]] std::span<std::byte> Mapped() const noexcept { return m_Mapped ? std::span<std::byte>(m_Mapped, m_Size) : std::span<std::byte>{}; }

        /** Graphics-timeline synchronisation state, maintained by the render graph. */
        [[nodiscard]] ResourceState& State() noexcept { return m_State; }

    private:
        /** Throws std::out_of_range when [Offset, Offset + Size) is out of bounds or the buffer is empty. */
        void CheckRange(VkDeviceSize Offset, VkDeviceSize Size) const;
    };
}
