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
    class Memory;
    class Buffer;
    class Device;

    /**
     * Arguments for image and image view creation.
     */
    struct LUVK_API ImageCreationArguments
    {
        /** The width, height, and depth/layer count of the image. */
        VkExtent3D Extent{0U, 0U, 1U};

        /** The pixel format of the image. */
        VkFormat Format{VK_FORMAT_R8G8B8A8_UNORM};

        /** Vulkan usage flags (e.g., VK_IMAGE_USAGE_SAMPLED_BIT). */
        VkImageUsageFlags Usage{VK_IMAGE_USAGE_FLAG_BITS_MAX_ENUM};

        /** Aspect flags for the image view (e.g., VK_IMAGE_ASPECT_COLOR_BIT). */
        VkImageAspectFlags Aspect{VK_IMAGE_ASPECT_COLOR_BIT};

        /** VMA memory usage (e.g., VMA_MEMORY_USAGE_GPU_ONLY). */
        VmaMemoryUsage MemoryUsage{VMA_MEMORY_USAGE_AUTO};

        /** Allocation priority (1.0 is default/highest). */
        float Priority{1.F};

        /** Optional debug name for the image. */
        std::string Name{};
    };

    /**
     * Wrapper for a Vulkan Image and its VMA allocation and ImageView.
     */
    class LUVK_API Image
    {
        /** Width of the image. */
        std::uint32_t m_Width{0U};

        /** Height of the image. */
        std::uint32_t m_Height{0U};

        /** The Vulkan image handle. */
        VkImage m_Image{VK_NULL_HANDLE};

        /** The Vulkan image view handle. */
        VkImageView m_View{VK_NULL_HANDLE};

        /** The VMA allocation handle. */
        VmaAllocation m_Allocation{nullptr};

        /** Pointer to the Memory module for allocation. */
        Memory* m_MemoryModule{nullptr};

        /** Pointer to the Device module for immediate command submission. */
        Device* m_DeviceModule{nullptr};

        /** Index in a bindless texture array, if used. */
        std::int32_t m_BindlessIndex{-1};

    public:
        /** Images cannot be default constructed. */
        Image() = delete;

        /**
         * Constructor.
         * @param DeviceModule Pointer to the Device module.
         * @param MemoryModule Pointer to the Memory module.
         */
        explicit Image(Device* DeviceModule, Memory* MemoryModule);

        /** Destructor (destroys the image view and frees VMA allocation). */
        ~Image();

        /**
         * Creates or recreates the image and its view.
         * @param Arguments Configuration for the image.
         */
        void CreateImage(const ImageCreationArguments& Arguments);

        /**
         * Uploads data to the image using a staging buffer and transfer commands.
         * @param Data Raw image data to upload.
         */
        void Upload(std::span<const std::byte> Data) const;

        /** Get the underlying VkImage handle. */
        [[nodiscard]] constexpr VkImage GetHandle() const noexcept
        {
            return m_Image;
        }

        /** Get the underlying VkImageView handle. */
        [[nodiscard]] constexpr VkImageView GetView() const noexcept
        {
            return m_View;
        }

        /** Get the extent (resolution) of the image. */
        [[nodiscard]] constexpr VkExtent3D GetExtent() const noexcept
        {
            return {m_Width, m_Height, 1U};
        }

        /** Set the bindless index of this image. */
        constexpr void SetBindlessIndex(const std::int32_t Index) noexcept
        {
            m_BindlessIndex = Index;
        }

        /** Get the bindless index of this image. */
        [[nodiscard]] constexpr std::int32_t GetBindlessIndex() const noexcept
        {
            return m_BindlessIndex;
        }
    };
}
