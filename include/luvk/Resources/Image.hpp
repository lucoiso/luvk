// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstddef>
#include <memory>
#include <span>
#include <vk_mem_alloc.h>

namespace luvk
{
    class Memory;
    class Buffer;
    class Device;

    class LUVK_API Image
    {
    protected:
        std::uint32_t           m_Width{0};
        std::uint32_t           m_Height{0};
        VkImage                 m_Image{VK_NULL_HANDLE};
        VkImageView             m_View{VK_NULL_HANDLE};
        VmaAllocation           m_Allocation{};
        std::shared_ptr<Memory> m_MemoryModule{};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        Image() = delete;
        explicit Image(const std::shared_ptr<Device>& DeviceModule, const std::shared_ptr<Memory>& MemoryModule);

        ~Image();

        struct CreationArguments
        {
            VkExtent3D         Extent{0, 0, 1};
            VkFormat           Format{VK_FORMAT_R8G8B8A8_UNORM};
            VkImageUsageFlags  Usage{};
            VkImageAspectFlags Aspect{VK_IMAGE_ASPECT_COLOR_BIT};
            VmaMemoryUsage     MemoryUsage{VMA_MEMORY_USAGE_AUTO};
            float              Priority{1.F};
        };

        void CreateImage(const CreationArguments& Arguments);

        void Upload(std::span<const std::byte> Data) const;
        void Upload(const std::shared_ptr<Buffer>& Staging) const;

        [[nodiscard]] constexpr VkImage GetHandle() const noexcept
        {
            return m_Image;
        }

        [[nodiscard]] constexpr VkImageView GetView() const noexcept
        {
            return m_View;
        }
    };
} // namespace luvk
