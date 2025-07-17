// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include <memory>

#include <vma/vk_mem_alloc.h>
#include <span>

namespace luvk
{
    /** Buffer object managed with VMA */
    class Memory;

    class LUVKMODULE_API Buffer
    {
        /** Handle to the allocated buffer */
        VkBuffer m_Buffer{VK_NULL_HANDLE};

        /** Allocation handle returned by VMA */
        VmaAllocation m_Allocation{nullptr};

        /** Total size in bytes */
        VkDeviceSize m_Size{0};

        /** Owning memory module */
        std::shared_ptr<Memory> m_MemoryModule{};

    public: /** Default constructor */
        constexpr Buffer() = default;

        /** Release the buffer and its allocation */
        ~Buffer();

        /** Buffer creation information */
        struct CreationArguments
        {
            /** Size in bytes */
            VkDeviceSize Size{0};

            /** Usage flags describing how the buffer will be used */
            VkBufferUsageFlags Usage{};

            /** Desired memory usage */
            VmaMemoryUsage MemoryUsage{VMA_MEMORY_USAGE_AUTO};

            /** Memory priority (0.0 - 1.0) */
            float Priority{1.F};
        };

        /** Create buffer using allocator */
        void CreateBuffer(std::shared_ptr<Memory> const& MemoryModule, CreationArguments const& Arguments);

        /** Recreate buffer destroying previous allocation */
        void RecreateBuffer(CreationArguments const& Arguments);

        /** Map memory and copy data */
        void Upload(std::span<const std::byte> Data) const;

        /** Retrieve the allocated Vulkan buffer handle */
        [[nodiscard]] constexpr VkBuffer const& GetHandle() const
        {
            return m_Buffer;
        }

        /** Get buffer size */
        [[nodiscard]] constexpr VkDeviceSize GetSize() const
        {
            return m_Size;
        }
    };
} // namespace luvk
