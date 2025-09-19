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
    class Device;
    class Memory;

    class LUVKMODULE_API Buffer
    {
        VkBuffer m_Buffer{VK_NULL_HANDLE};
        VmaAllocation m_Allocation{nullptr};
        VkDeviceSize m_Size{0};
        std::shared_ptr<Device> m_DeviceModule{};
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

        void CreateBuffer(const std::shared_ptr<Memory>& MemoryModule, const CreationArguments& Arguments);
        void RecreateBuffer(const CreationArguments& Arguments);
        void Upload(const std::span<const std::byte>& Data) const;

        [[nodiscard]] constexpr const VkBuffer& GetHandle() const
        {
            return m_Buffer;
        }

        [[nodiscard]] constexpr VkDeviceSize GetSize() const
        {
            return m_Size;
        }
    };
} // namespace luvk
