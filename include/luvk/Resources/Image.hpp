// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vma/vk_mem_alloc.h>
#include "luvk/Module.hpp"

namespace luvk
{
    class Memory;
    class Buffer;
    class Device;

    class LUVKMODULE_API Image
    {
        std::uint32_t m_Width{0};
        std::uint32_t m_Height{0};
        VkImage m_Image{VK_NULL_HANDLE};
        VkImageView m_View{VK_NULL_HANDLE};
        VmaAllocation m_Allocation{nullptr};
        std::shared_ptr<Memory> m_MemoryModule{};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        constexpr Image() = default;
        ~Image();

        struct CreationArguments
        {
            VkExtent3D Extent{0, 0, 1};
            VkFormat Format{VK_FORMAT_R8G8B8A8_UNORM};
            VkImageUsageFlags Usage{};
            VkImageAspectFlags Aspect{VK_IMAGE_ASPECT_COLOR_BIT};
            VmaMemoryUsage MemoryUsage{VMA_MEMORY_USAGE_AUTO};
            float Priority{1.F};
        };

        void CreateImage(const std::shared_ptr<Device>& DeviceModule, const std::shared_ptr<Memory>& MemoryModule, const CreationArguments& Arguments);

        void Upload(const std::span<const std::byte>& Data) const;
        void Upload(const std::shared_ptr<Buffer>& Staging);

        [[nodiscard]] constexpr const VkImage& GetHandle() const
        {
            return m_Image;
        }

        [[nodiscard]] constexpr const VkImageView& GetView() const
        {
            return m_View;
        }

    private:
        void Destroy();
    };
} // namespace luvk
