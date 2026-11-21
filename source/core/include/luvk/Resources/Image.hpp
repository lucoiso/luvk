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
#include "luvk/Core/Handles.hpp"
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
        Offset3D Offset = {};

        /** Size of the box in texels; a zero width means the whole mip level. */
        Extent3D Extent = {0U, 0U, 0U};

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
        VkImageView View = nullptr;

        /** Index into Textures[] (binding 0), or InvalidBindlessIndex without sampled usage. */
        std::uint32_t SampledIndex = InvalidBindlessIndex;

        /** Index into Images[] (binding 2), or InvalidBindlessIndex without storage usage or for several mip levels. */
        std::uint32_t StorageIndex = InvalidBindlessIndex;

        /** Storage handle of the view. */
        [[nodiscard]] constexpr StorageImage Storage() const noexcept { return {StorageIndex}; }

        /** Sampled handle of the view as a 2D texture. */
        [[nodiscard]] constexpr Texture2D Sampled() const noexcept { return {SampledIndex}; }
    };

    /** Creation parameters for luvk::Image. */
    struct ImageDesc
    {
        /** Image extent in texels; depth > 1 creates a 3D image. */
        Extent3D Extent = {1U, 1U, 1U};

        /** Texel format. */
        luvk::Format Format = luvk::Format::R8G8B8A8Unorm;

        /** Usage; Auto derives it from the format (sampled, transfer, storage when supported, color or depth attachment). */
        ImageUsage Usage = ImageUsage::Auto;

        /** Number of mip levels (1 or more). */
        std::uint32_t MipLevels = 1U;

        /** Number of array layers (1 or more, multiple of 6 for cube maps). */
        std::uint32_t ArrayLayers = 1U;

        /** Rasterization sample count for multisampled images. */
        SampleCount Samples = SampleCount::X1;

        /** Create a cube or cube-array compatible image (requires square faces). */
        bool Cube = false;

        /** Set for images only ever touched by the graphics queue (e.g. render graph transients): they stay exclusive to
         *  one queue family, which keeps framebuffer compression enabled on some GPUs. Such images cannot be uploaded by a
         *  dedicated transfer queue nor used by the async compute graph on a dedicated compute family. */
        bool GraphicsQueueOnly = false;

        /** VMA allocation priority in [0, 1]. */
        float Priority = 1.F;

        /** Optional debug name applied to the image, its view and the allocation. */
        std::string_view Name;
    };

    /** True when Value includes a depth aspect. */
    [[nodiscard]] LUVK_API bool IsDepthFormat(luvk::Format Value) noexcept;

    /** True when Value includes a stencil aspect. */
    [[nodiscard]] LUVK_API bool HasStencil(luvk::Format Value) noexcept;

    /** Returns the bytes needed to store a region of Value (block-compressed formats included), or 0 for formats luvk does not know. */
    [[nodiscard]] LUVK_API DeviceSize FormatRegionSize(luvk::Format Value, Extent3D Extent) noexcept;

    /** Move-only RAII image with a default view. Images with sampled / storage usage are registered in the bindless heap
     *  automatically (see Sampled() / Storage()), and the slots are recycled through the deletion queue once the GPU is
     *  done with the image. */
    class LUVK_API Image
    {
        /** Owning Context used for allocation, uploads, bindless management and retirement. */
        Context* m_Context = nullptr;

        /** Vulkan image handle. */
        VkImage m_Image = nullptr;

        /** VMA allocation backing the image. */
        VmaAllocation m_Allocation = nullptr;

        /** Default view covering all mip levels and array layers of the image aspect. */
        VkImageView m_View = nullptr;

        /** Depth-only view created for combined depth/stencil sampled images, otherwise nullptr. */
        VkImageView m_SampledView = nullptr;

        /** Views of single mip levels / array layers, created on demand (stable addresses). */
        std::vector<std::unique_ptr<ImageSubview>> m_Subviews;

        /** Image extent in texels. */
        Extent3D m_Extent = {};

        /** Texel format. */
        luvk::Format m_Format = luvk::Format::Undefined;

        /** Usage the image was created with. */
        ImageUsage m_Usage = ImageUsage::Auto;

        /** VkImageAspectFlags derived from the format. */
        std::uint32_t m_Aspect = 0U;

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

        /** True when the image is shared by every queue family luvk submits to. */
        bool m_Concurrent = false;

        /** True for cube-compatible images. */
        bool m_Cube = false;

        /** Graphics-timeline synchronisation state of the whole image. */
        ResourceState m_State = {};

    public:
        /** Default constructor. */
        Image() = default;

        /** Constructor. Creates the image and its default view; registers bindless slots for sampled and storage usage. */
        Image(Context& Ctx, const ImageDesc& Desc);

        /** Destructor. */
        ~Image();

        /** Non-copyable. */
        Image(const Image&) = delete;

        /** Non-copyable. */
        Image& operator=(const Image&) = delete;

        /** Move-only. */
        Image(Image&& Other) noexcept;

        /** Move-only. */
        Image& operator=(Image&& Other) noexcept;

        /** Retires the image, its views and its bindless slots (deferred) and leaves this object empty. */
        void Reset() noexcept;

        /** Uploads tightly packed texels into one mip level / array layer through the transfer queue; afterwards the image
         *  is ready to be sampled, and the copy waits for the graphics work that last used the image. */
        void Upload(std::span<const std::byte> Data, std::uint32_t MipLevel = 0U, std::uint32_t ArrayLayer = 0U);

        /** Uploads a box of texels (e.g. the dirty rectangle of a font atlas); the rest of the image is preserved. */
        void Upload(std::span<const std::byte> Data, const ImageRegion& Region);

        /** True when a VkImage was created. */
        [[nodiscard]] explicit operator bool() const noexcept { return m_Image != nullptr; }

        /** Underlying VkImage handle. */
        [[nodiscard]] VkImage Handle() const noexcept { return m_Image; }

        /** Default full-resource view. */
        [[nodiscard]] VkImageView View() const noexcept { return m_View; }

        /** View used for sampling (depth-only for combined depth/stencil images). */
        [[nodiscard]] VkImageView SampledView() const noexcept { return m_SampledView != nullptr ? m_SampledView : m_View; }

        /** Image extent in texels. */
        [[nodiscard]] Extent3D Extent() const noexcept { return m_Extent; }

        /** Width and height of the image extent. */
        [[nodiscard]] luvk::Extent2D Extent2D() const noexcept { return {m_Extent.Width, m_Extent.Height}; }

        /** Texel format. */
        [[nodiscard]] luvk::Format Format() const noexcept { return m_Format; }

        /** Usage the image was created with (resolved when the description asked for Auto). */
        [[nodiscard]] ImageUsage Usage() const noexcept { return m_Usage; }

        /** VkImageAspectFlags derived from the format. */
        [[nodiscard]] std::uint32_t Aspect() const noexcept { return m_Aspect; }

        /** Number of mip levels. */
        [[nodiscard]] std::uint32_t MipLevels() const noexcept { return m_MipLevels; }

        /** Number of array layers. */
        [[nodiscard]] std::uint32_t ArrayLayers() const noexcept { return m_ArrayLayers; }

        /** True for 3D images. */
        [[nodiscard]] bool Is3D() const noexcept { return m_Extent.Depth > 1U; }

        /** True for cube-compatible images. */
        [[nodiscard]] bool IsCube() const noexcept { return m_Cube; }

        /** True when the graphics and transfer queues may both access the image without ownership transfers. */
        [[nodiscard]] bool IsSharedWithTransferQueue() const noexcept;

        /** True when the graphics and async compute queues may both access the image without ownership transfers. */
        [[nodiscard]] bool IsSharedWithComputeQueue() const noexcept;

        /** True when the image is shared by every queue family luvk submits to. */
        [[nodiscard]] bool IsConcurrent() const noexcept { return m_Concurrent; }

        /** Records that a transfer batch (timeline value) uses this image; destruction then also waits for it. */
        void NoteTransferUse(std::uint64_t TransferValue) noexcept;

        /** Index into Textures[] (binding 0) or InvalidBindlessIndex. */
        [[nodiscard]] std::uint32_t SampledIndex() const noexcept { return m_SampledIndex; }

        /** Index into Images[] (binding 2) or InvalidBindlessIndex (storage access to several mip levels goes through
         *  Subview(Mip).StorageIndex). */
        [[nodiscard]] std::uint32_t StorageIndex() const noexcept { return m_StorageIndex; }

        /** Sampled handle of a 2D image (throws std::logic_error for 3D, cube or array images). */
        [[nodiscard]] Texture2D Sampled() const;

        /** Sampled handle of a 3D image (throws std::logic_error otherwise). */
        [[nodiscard]] Texture3D Sampled3D() const;

        /** Sampled handle of a cube image (throws std::logic_error otherwise). */
        [[nodiscard]] TextureCube SampledCube() const;

        /** Sampled handle of a 2D array image (throws std::logic_error otherwise). */
        [[nodiscard]] TextureArray SampledArray() const;

        /** Storage handle of the whole image (single-mip images; other mips through Subview). */
        [[nodiscard]] StorageImage Storage() const noexcept { return {m_StorageIndex}; }

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
