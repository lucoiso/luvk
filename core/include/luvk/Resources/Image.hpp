// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>
#include "luvk/Core/Common.hpp"
#include "luvk/Core/Vma.hpp"
#include "luvk/Resources/ResourceState.hpp"

namespace luvk
{
    class Context;

    /** Destination of an image upload: a box inside one mip level / array layer. */
    struct ImageRegion
    {
        /** Mip level written. */
        std::uint32_t MipLevel = 0U;

        /** Array layer written. */
        std::uint32_t ArrayLayer = 0U;

        /** First texel of the box. */
        VkOffset3D Offset = {};

        /** Size of the box in texels; a zero width means the whole mip level. */
        VkExtent3D Extent = {};

        /** Texels per row of the source data when it is wider than the box (0 = tightly packed). */
        std::uint32_t RowLength = 0U;
    };

    /** View of one mip level and/or array layer of an Image, with its own bindless slots. */
    struct ImageSubview
    {
        /** Mip level covered, or AllSubresources. */
        std::uint32_t MipLevel = AllSubresources;

        /** Array layer covered, or AllSubresources. */
        std::uint32_t ArrayLayer = AllSubresources;

        /** Vulkan view of the subresources. */
        VkImageView View = VK_NULL_HANDLE;

        /** Index into Textures[] (binding 0), or InvalidBindlessIndex without SAMPLED usage. */
        std::uint32_t SampledIndex = InvalidBindlessIndex;

        /** Index into Images[] (binding 2), or InvalidBindlessIndex without STORAGE usage or for several mip levels. */
        std::uint32_t StorageIndex = InvalidBindlessIndex;
    };

    /** Creation parameters for luvk::Image. */
    struct ImageDesc
    {
        /** Image extent in texels; depth > 1 creates a 3D image. */
        VkExtent3D Extent = {1U, 1U, 1U};

        /** Texel format. */
        VkFormat Format = VK_FORMAT_R8G8B8A8_UNORM;

        /** VkImageUsageFlags for the image and its views. */
        VkImageUsageFlags Usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        /** Number of mip levels (1 or more). */
        std::uint32_t MipLevels = 1U;

        /** Number of array layers (1 or more, multiple of 6 for cube maps). */
        std::uint32_t ArrayLayers = 1U;

        /** Rasterization sample count for multisampled images. */
        VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;

        /** Create a cube or cube-array compatible image (requires square faces). */
        bool Cube = false;

        /** Set for images only ever touched by the graphics queue (e.g. render graph transients): they stay VK_SHARING_MODE_EXCLUSIVE, which keeps framebuffer compression enabled on some GPUs. */
        bool GraphicsQueueOnly = false;

        /** VMA allocation priority in [0, 1]. */
        float Priority = 1.F;

        /** Optional debug name applied to the image, its view and the allocation. */
        std::string_view Name;
    };

    /** Returns the aspect mask matching Format: color, depth, depth combined with stencil, or stencil. */
    [[nodiscard]] LUVK_API VkImageAspectFlags AspectFromFormat(VkFormat Format) noexcept;

    /** True when Format includes a depth aspect. */
    [[nodiscard]] LUVK_API bool               IsDepthFormat(VkFormat Format) noexcept;

    /** True when Format includes a stencil aspect. */
    [[nodiscard]] LUVK_API bool               HasStencil(VkFormat Format) noexcept;

    /** Returns the bytes needed to store a Width x Height x Depth region of Format (block-compressed formats included),
     *  or 0 for formats luvk does not know. */
    [[nodiscard]] LUVK_API VkDeviceSize FormatRegionSize(VkFormat Format, VkExtent3D Extent) noexcept;

    /** Move-only RAII image with a default view. Images with SAMPLED / STORAGE usage are registered in the bindless heap
     *  automatically (see SampledIndex() / StorageIndex()), and the slots are recycled through the deletion queue once the GPU is done with the image. */
    class LUVK_API Image
    {
        /** Owning Context used for allocation, uploads, bindless management and retirement. */
        Context* m_Context = nullptr;

        /** Vulkan image handle. */
        VkImage m_Image = VK_NULL_HANDLE;

        /** VMA allocation backing the image. */
        VmaAllocation m_Allocation = nullptr;

        /** Default view covering all mip levels and array layers of the image aspect. */
        VkImageView m_View = VK_NULL_HANDLE;

        /** Depth-only view created for combined depth/stencil sampled images, otherwise VK_NULL_HANDLE. */
        VkImageView m_SampledView = VK_NULL_HANDLE;

        /** Views of single mip levels / array layers, created on demand (stable addresses). */
        std::vector<std::unique_ptr<ImageSubview>> m_Subviews;

        /** Image extent in texels. */
        VkExtent3D m_Extent = {};

        /** Texel format. */
        VkFormat m_Format = VK_FORMAT_UNDEFINED;

        /** VkImageUsageFlags the image was created with. */
        VkImageUsageFlags m_Usage = 0U;

        /** Aspect mask derived from the format. */
        VkImageAspectFlags m_Aspect = 0U;

        /** Number of mip levels. */
        std::uint32_t m_MipLevels = 1U;

        /** Number of array layers. */
        std::uint32_t m_ArrayLayers = 1U;

        /** Bindless slot in Textures[] (binding 0), or InvalidBindlessIndex. */
        std::uint32_t m_SampledIndex = InvalidBindlessIndex;

        /** Bindless slot in Images[] (binding 2), or InvalidBindlessIndex. */
        std::uint32_t m_StorageIndex = InvalidBindlessIndex;

        /** Highest transfer-queue timeline value using this image; destruction also waits for it. */
        std::uint64_t m_TransferUse = 0U;

        /** True when the image was created with VK_SHARING_MODE_CONCURRENT. */
        bool m_Concurrent = false;

        /** True for cube-compatible images. */
        bool m_Cube = false;

        /** Graphics-timeline synchronisation state of the whole image. */
        ResourceState m_State = {};

    public:
        /** Default constructor. */
        Image() = default;

        /** Constructor. Creates the image and its default view; registers bindless slots when usage includes SAMPLED or STORAGE. */
        Image(Context& Ctx, const ImageDesc& Desc);

        /** Destructor. */
        ~Image();

        /** Non-copyable. */
        Image(const Image&)            = delete;

        /** Non-copyable. */
        Image& operator=(const Image&) = delete;

        /** Move-only. */
        Image(Image&& Other) noexcept;

        /** Move-only. */
        Image& operator=(Image&& Other) noexcept;

        /** Retires the image, its views and its bindless slots (deferred) and leaves this object empty. */
        void Reset() noexcept;

        /** Uploads tightly packed texels into one mip level / array layer through the transfer queue; afterwards the image is in
         *  VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL and the copy waits for previously submitted graphics work, so re-uploading in-flight images is safe. */
        void Upload(std::span<const std::byte> Data, std::uint32_t MipLevel = 0U, std::uint32_t ArrayLayer = 0U);

        /** Uploads a box of texels (e.g. the dirty rectangle of a font atlas); the rest of the image is preserved. */
        void Upload(std::span<const std::byte> Data, const ImageRegion& Region);

        /** True when a VkImage was created. */
        [[nodiscard]] explicit operator bool() const noexcept { return m_Image != VK_NULL_HANDLE; }

        /** Underlying VkImage handle. */
        [[nodiscard]] VkImage            Handle() const noexcept { return m_Image; }

        /** Default full-resource view. */
        [[nodiscard]] VkImageView        View() const noexcept { return m_View; }

        /** View used for sampling (depth-only for combined depth/stencil images). */
        [[nodiscard]] VkImageView        SampledView() const noexcept { return m_SampledView != VK_NULL_HANDLE ? m_SampledView : m_View; }

        /** Image extent in texels. */
        [[nodiscard]] VkExtent3D         Extent() const noexcept { return m_Extent; }

        /** Width and height of the image extent. */
        [[nodiscard]] VkExtent2D         Extent2D() const noexcept { return {m_Extent.width, m_Extent.height}; }

        /** Texel format. */
        [[nodiscard]] VkFormat           Format() const noexcept { return m_Format; }

        /** VkImageUsageFlags the image was created with. */
        [[nodiscard]] VkImageUsageFlags  Usage() const noexcept { return m_Usage; }

        /** Aspect mask derived from the format. */
        [[nodiscard]] VkImageAspectFlags Aspect() const noexcept { return m_Aspect; }

        /** Number of mip levels. */
        [[nodiscard]] std::uint32_t      MipLevels() const noexcept { return m_MipLevels; }

        /** Number of array layers. */
        [[nodiscard]] std::uint32_t      ArrayLayers() const noexcept { return m_ArrayLayers; }

        /** True when the graphics and transfer queues may both access the image without ownership transfers. */
        [[nodiscard]] bool IsSharedWithTransferQueue() const noexcept;

        /** Records that a transfer batch (timeline value) uses this image; destruction then also waits for it. */
        void NoteTransferUse(std::uint64_t TransferValue) noexcept;

        /** Index into Textures[] (binding 0) or InvalidBindlessIndex. */
        [[nodiscard]] std::uint32_t SampledIndex() const noexcept { return m_SampledIndex; }

        /** Index into Images[] (binding 2) or InvalidBindlessIndex (storage access to several mip levels goes through
         *  Subview(Mip).StorageIndex). */
        [[nodiscard]] std::uint32_t StorageIndex() const noexcept { return m_StorageIndex; }

        /**
         * View of one mip level and/or array layer (AllSubresources keeps the whole range of that dimension), created on
         * first use and registered in the bindless heap: sample or write a single mip (depth pyramids, bloom chains,
         * mip generation) or render into one layer (shadow cascades, cube faces).
         */
        [[nodiscard]] const ImageSubview& Subview(std::uint32_t MipLevel, std::uint32_t ArrayLayer = AllSubresources);

        /** Graphics-timeline synchronisation state (whole image), maintained by the render graph and uploads. */
        [[nodiscard]] ResourceState& State() noexcept { return m_State; }
    };
}
