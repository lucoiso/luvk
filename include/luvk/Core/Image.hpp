// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

#include <vma/vk_mem_alloc.h>
#include <span>
#include <cstddef>
#include <memory>

namespace luvk
{
    /** Image object managed with VMA */
    class Memory;
    class Device;

    class LUVKMODULE_API Image
    {
        /** Handle to the Vulkan image */
        VkImage m_Image{VK_NULL_HANDLE};

        /** View into the image */
        VkImageView m_View{VK_NULL_HANDLE};

        /** Allocation handle from VMA */
        VmaAllocation m_Allocation{nullptr};

        /** Memory module owning the allocation */
        std::shared_ptr<Memory> m_MemoryModule{};

        /** Device module used to create the image */
        std::shared_ptr<Device> m_DeviceModule{};

    public: /** Default constructor */
        constexpr Image() = default;

        /** Release image resources */
        ~Image();

        /** Parameters required to create the image */
        struct CreationArguments
        {
            /** Dimensions of the image */
            VkExtent3D Extent{0, 0, 1};

            /** Format of the image */
            VkFormat Format{VK_FORMAT_R8G8B8A8_UNORM};

            /** Usage flags describing how the image will be used */
            VkImageUsageFlags Usage{};

            /** Aspect mask for the default view */
            VkImageAspectFlags Aspect{VK_IMAGE_ASPECT_COLOR_BIT};

            /** Desired memory usage */
            VmaMemoryUsage MemoryUsage{VMA_MEMORY_USAGE_AUTO};
        };

        void CreateImage(std::shared_ptr<Device> const& DeviceModule, std::shared_ptr<Memory> const& MemoryModule, CreationArguments const& Arguments);

        /** Upload data to the image memory */
        void Upload(std::span<const std::byte> Data) const;

        /** Retrieve the Vulkan image handle */
        [[nodiscard]] constexpr VkImage const& GetHandle() const
        {
            return m_Image;
        }

        /** Retrieve the Vulkan image view handle */
        [[nodiscard]] constexpr VkImageView const& GetView() const
        {
            return m_View;
        }

    private: /** Destroy the image and view */
        void Destroy();
    };
} // namespace luvk
