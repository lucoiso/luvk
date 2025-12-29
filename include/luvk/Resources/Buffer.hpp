/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <span>
#include <string>
#include <vk_mem_alloc.h>

namespace luvk
{
    class Device;
    class Memory;

    /**
     * Arguments for buffer creation.
     */
    struct LUVK_API BufferCreationArguments
    {
        /** Size of the buffer in bytes. */
        VkDeviceSize Size{0U};

        /** Vulkan usage flags (e.g., VK_BUFFER_USAGE_VERTEX_BUFFER_BIT). */
        VkBufferUsageFlags Usage{VK_BUFFER_USAGE_FLAG_BITS_MAX_ENUM};

        /** VMA memory usage (e.g., VMA_MEMORY_USAGE_GPU_ONLY). */
        VmaMemoryUsage MemoryUsage{VMA_MEMORY_USAGE_AUTO};

        /** Allocation priority (1.0 is default/highest). */
        float Priority{1.F};

        /** If true, the memory will be persistently mapped. */
        bool PersistentlyMapped{false};

        /** Optional debug name for the buffer. */
        std::string Name{};
    };

    /**
     * Wrapper for a Vulkan Buffer with VMA allocation.
     * Supports Buffer Device Address (BDA) for Vulkan 1.4.
     */
    class LUVK_API Buffer
    {
        /** Flag indicating if the buffer is persistently mapped. */
        bool m_IsPersistentMap{false};

        /** The Vulkan buffer handle. */
        VkBuffer m_Buffer{VK_NULL_HANDLE};

        /** The VMA allocation handle. */
        VmaAllocation m_Allocation{nullptr};

        /** Pointer to the persistently mapped memory, if any. */
        void* m_Map{nullptr};

        /** Size of the buffer in bytes. */
        VkDeviceSize m_Size{0U};

        /** Buffer Device Address (BDA) if enabled. */
        VkDeviceAddress m_DeviceAddress{0U};

        /** Pointer to the Device module for getting the logical device. */
        Device* m_DeviceModule{nullptr};

        /** Pointer to the Memory module for allocation. */
        Memory* m_MemoryModule{nullptr};

    public:
        /** Buffers cannot be default constructed. */
        Buffer() = delete;

        /**
         * Constructor.
         * @param DeviceModule Pointer to the Device module.
         * @param MemoryModule Pointer to the Memory module.
         */
        explicit Buffer(Device* DeviceModule, Memory* MemoryModule);

        /** Destructor (destroys the buffer and frees VMA allocation). */
        ~Buffer();

        /**
         * Create or recreate the buffer with the given arguments.
         * @param Arguments Configuration for the buffer.
         */
        void CreateBuffer(const BufferCreationArguments& Arguments);

        /**
         * Upload data to the buffer.
         * Uses internal staging or direct copy depending on memory type.
         * @param Data Raw data to upload.
         * @param Offset Byte offset in the buffer.
         */
        void Upload(std::span<const std::byte> Data, std::uint64_t Offset = 0U) const;

        /** Get the underlying VkBuffer handle. */
        [[nodiscard]] constexpr VkBuffer GetHandle() const noexcept
        {
            return m_Buffer;
        }

        /** Get the size of the buffer in bytes. */
        [[nodiscard]] constexpr VkDeviceSize GetSize() const noexcept
        {
            return m_Size;
        }

        /** Get the pointer to the mapped memory (only valid if persistently mapped). */
        [[nodiscard]] constexpr void* GetMappedData() const noexcept
        {
            return m_Map;
        }

        /**
         * Get the GPU address of the buffer (requires VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT).
         * @return The 64-bit device address.
         */
        [[nodiscard]] constexpr VkDeviceAddress GetDeviceAddress() const noexcept
        {
            return m_DeviceAddress;
        }
    };
}
