// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <string>
#include <vk_mem_alloc.h>
#include "luvk/Module.hpp"

namespace luvk
{
    class Device;
    class Memory;

    class LUVKMODULE_API Buffer
    {
    protected:
        VkBuffer                m_Buffer{VK_NULL_HANDLE};
        VmaAllocation           m_Allocation{};
        void*                   m_Map{nullptr};
        VkDeviceSize            m_Size{0};
        std::shared_ptr<Device> m_DeviceModule{};
        std::shared_ptr<Memory> m_MemoryModule{};

    public:
        Buffer() = delete;
        explicit Buffer(const std::shared_ptr<Device>& DeviceModule, const std::shared_ptr<Memory>& MemoryModule);

        ~Buffer();

        struct CreationArguments
        {
            VkDeviceSize       Size{0};
            VkBufferUsageFlags Usage{};
            VmaMemoryUsage     MemoryUsage{VMA_MEMORY_USAGE_AUTO};
            float              Priority{1.F};
            std::string        Name{};
        };

        void CreateBuffer(const CreationArguments& Arguments);
        void RecreateBuffer(const CreationArguments& Arguments);
        void Upload(std::span<const std::byte> Data) const;

        [[nodiscard]] constexpr VkBuffer GetHandle() const noexcept
        {
            return m_Buffer;
        }

        [[nodiscard]] constexpr VkDeviceSize GetSize() const noexcept
        {
            return m_Size;
        }
    };
} // namespace luvk
