// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <vma/vk_mem_alloc.h>
#include "luvk/Module.hpp"

namespace luvk
{
    class Memory;

    class LUVKMODULE_API Buffer
    {
        VkBuffer m_Buffer{VK_NULL_HANDLE};
        VmaAllocation m_Allocation{nullptr};
        VkDeviceSize m_Size{0};
        std::shared_ptr<Memory> m_MemoryModule{};

    public:
        constexpr Buffer() = default;
        ~Buffer();

        struct CreationArguments
        {
            VkDeviceSize Size{0};
            VkBufferUsageFlags Usage{};
            VmaMemoryUsage MemoryUsage{VMA_MEMORY_USAGE_AUTO};
            float Priority{1.F};
        };

        void CreateBuffer(std::shared_ptr<Memory> const& MemoryModule, CreationArguments const& Arguments);
        void RecreateBuffer(CreationArguments const& Arguments);
        void Upload(std::span<const std::byte> Data) const;

        [[nodiscard]] constexpr VkBuffer const& GetHandle() const
        {
            return m_Buffer;
        }

        [[nodiscard]] constexpr VkDeviceSize GetSize() const
        {
            return m_Size;
        }
    };
} // namespace luvk
