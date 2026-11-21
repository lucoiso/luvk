// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "ImageInternals.hpp"
#include "luvk/Resources/Image.hpp"
#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Upload.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace
{

    luvk::FormatInfo DescribeFormat(const VkFormat Format) noexcept
    {
        switch (Format)
        {
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8_SNORM:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8_SINT:
        case VK_FORMAT_R8_SRGB:
        case VK_FORMAT_S8_UINT: return {1U};

        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8_SNORM:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8_SINT:
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16_SNORM:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16_SINT:
        case VK_FORMAT_R16_SFLOAT:
        case VK_FORMAT_D16_UNORM:
        case VK_FORMAT_R5G6B5_UNORM_PACK16:
        case VK_FORMAT_B5G6R5_UNORM_PACK16:
        case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
        case VK_FORMAT_B4G4R4A4_UNORM_PACK16: return {2U};

        case VK_FORMAT_D16_UNORM_S8_UINT: return {3U};

        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_R8G8B8A8_SNORM:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R8G8B8A8_SINT:
        case VK_FORMAT_R8G8B8A8_SRGB:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_SRGB:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
        case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16_SNORM:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16_SINT:
        case VK_FORMAT_R16G16_SFLOAT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32_SINT:
        case VK_FORMAT_R32_SFLOAT:
        case VK_FORMAT_D32_SFLOAT:
        case VK_FORMAT_X8_D24_UNORM_PACK32:
        case VK_FORMAT_D24_UNORM_S8_UINT: return {4U};

        case VK_FORMAT_D32_SFLOAT_S8_UINT: return {5U};

        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R16G16B16A16_SNORM:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R16G16B16A16_SINT:
        case VK_FORMAT_R16G16B16A16_SFLOAT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32_SINT:
        case VK_FORMAT_R32G32_SFLOAT: return {8U};

        case VK_FORMAT_R32G32B32_UINT:
        case VK_FORMAT_R32G32B32_SINT:
        case VK_FORMAT_R32G32B32_SFLOAT: return {12U};

        case VK_FORMAT_R32G32B32A32_UINT:
        case VK_FORMAT_R32G32B32A32_SINT:
        case VK_FORMAT_R32G32B32A32_SFLOAT: return {16U};

        case VK_FORMAT_BC1_RGB_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGB_SRGB_BLOCK:
        case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:
        case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:
        case VK_FORMAT_BC4_UNORM_BLOCK:
        case VK_FORMAT_BC4_SNORM_BLOCK: return {8U, 4U, 4U};

        case VK_FORMAT_BC2_UNORM_BLOCK:
        case VK_FORMAT_BC2_SRGB_BLOCK:
        case VK_FORMAT_BC3_UNORM_BLOCK:
        case VK_FORMAT_BC3_SRGB_BLOCK:
        case VK_FORMAT_BC5_UNORM_BLOCK:
        case VK_FORMAT_BC5_SNORM_BLOCK:
        case VK_FORMAT_BC6H_UFLOAT_BLOCK:
        case VK_FORMAT_BC6H_SFLOAT_BLOCK:
        case VK_FORMAT_BC7_UNORM_BLOCK:
        case VK_FORMAT_BC7_SRGB_BLOCK: return {16U, 4U, 4U};

        default: return {};
        }
    }

    VkImageViewType ViewType(const luvk::ImageDesc& Desc)
    {
        if (Desc.Extent.Depth > 1U)
        {
            return VK_IMAGE_VIEW_TYPE_3D;
        }
        if (Desc.Cube)
        {
            return Desc.ArrayLayers > 6U ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE;
        }
        return Desc.ArrayLayers > 1U ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    }
}

bool luvk::IsDepthFormat(const Format Value) noexcept
{
    switch (ToVk(Value))
    {
    case VK_FORMAT_D16_UNORM:
    case VK_FORMAT_X8_D24_UNORM_PACK32:
    case VK_FORMAT_D32_SFLOAT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
    default: return false;
    }
}

bool luvk::HasStencil(const Format Value) noexcept
{
    switch (ToVk(Value))
    {
    case VK_FORMAT_S8_UINT:
    case VK_FORMAT_D16_UNORM_S8_UINT:
    case VK_FORMAT_D24_UNORM_S8_UINT:
    case VK_FORMAT_D32_SFLOAT_S8_UINT: return true;
    default: return false;
    }
}

VkImageAspectFlags luvk::AspectFromFormat(const Format Value) noexcept
{
    if (Value == Format::S8Uint)
    {
        return VK_IMAGE_ASPECT_STENCIL_BIT;
    }

    if (IsDepthFormat(Value))
    {
        return HasStencil(Value) ? VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    return VK_IMAGE_ASPECT_COLOR_BIT;
}

luvk::DeviceSize luvk::FormatRegionSize(const Format Value, const Extent3D Extent) noexcept
{
    const luvk::FormatInfo Info = DescribeFormat(ToVk(Value));
    if (Info.BlockBytes == 0U)
    {
        return 0U;
    }

    const VkDeviceSize BlocksX = (Extent.Width + Info.BlockWidth - 1U) / Info.BlockWidth;
    const VkDeviceSize BlocksY = (Extent.Height + Info.BlockHeight - 1U) / Info.BlockHeight;
    return BlocksX * BlocksY * Extent.Depth * Info.BlockBytes;
}

luvk::Image::Image(Context& Ctx, const ImageDesc& Desc)
    : m_Context(&Ctx),
      m_Extent(Desc.Extent),
      m_Format(Desc.Format),
      m_Usage(Ctx.ResolveImageUsage(Desc.Format, Desc.Usage)),
      m_Aspect(AspectFromFormat(Desc.Format)),
      m_MipLevels(Desc.MipLevels),
      m_ArrayLayers(Desc.ArrayLayers),
      m_Cube(Desc.Cube)
{
    if (Desc.Extent.Width == 0U || Desc.Extent.Height == 0U || Desc.Extent.Depth == 0U || Desc.MipLevels == 0U || Desc.ArrayLayers == 0U)
    {
        throw std::invalid_argument("[luvk] Image extent, mip levels and array layers must be non-zero.");
    }

    if (Desc.Cube && (Desc.ArrayLayers % 6U != 0U || Desc.Extent.Width != Desc.Extent.Height))
    {
        throw std::invalid_argument("[luvk] Cube images need square faces and a multiple of 6 layers.");
    }

    const std::span<const std::uint32_t> Families   = Ctx.SharedQueueFamilies();
    const bool                           Concurrent = !Desc.GraphicsQueueOnly && std::size(Families) > 1U;
    m_Concurrent                                    = Concurrent;

    const VkImageCreateInfo Info{.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                                 .flags                 = Desc.Cube ? VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT : VkImageCreateFlags{0U},
                                 .imageType             = Desc.Extent.Depth > 1U ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
                                 .format                = ToVk(Desc.Format),
                                 .extent                = ToVk(Desc.Extent),
                                 .mipLevels             = Desc.MipLevels,
                                 .arrayLayers           = Desc.ArrayLayers,
                                 .samples               = ToVk(Desc.Samples),
                                 .tiling                = VK_IMAGE_TILING_OPTIMAL,
                                 .usage                 = ToVk(m_Usage),
                                 .sharingMode           = Concurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
                                 .queueFamilyIndexCount = Concurrent ? static_cast<std::uint32_t>(std::size(Families)) : 0U,
                                 .pQueueFamilyIndices   = Concurrent ? std::data(Families) : nullptr,
                                 .initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED};

    // Dedicated memory is left to VMA: it follows the driver's VK_KHR_dedicated_allocation preference per image.
    const VmaAllocationCreateInfo AllocationInfo{.usage    = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                                                 .priority = Desc.Priority};

    Check(vmaCreateImage(Ctx.Allocator(), &Info, &AllocationInfo, &m_Image, &m_Allocation, nullptr), "vmaCreateImage");
    Ctx.TrackResource(+1);

    constexpr VkImageUsageFlags ViewUsage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                            VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;

    try
    {
        // A VkImageView may only be created for images with at least one view-compatible usage.
        // Transfer-only transients (e.g. staging/copy targets) are valid images but cannot have a view.
        if ((ToVk(m_Usage) & ViewUsage) != 0U)
        {
            const VkImageViewType Type = ViewType(Desc);

            VkImageViewCreateInfo ViewInfo{.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                           .image            = m_Image,
                                           .viewType         = Type,
                                           .format           = ToVk(Desc.Format),
                                           .subresourceRange = {m_Aspect, 0U, VK_REMAINING_MIP_LEVELS, 0U, VK_REMAINING_ARRAY_LAYERS}};
            Check(vkCreateImageView(Ctx.Device(), &ViewInfo, nullptr, &m_View), "vkCreateImageView");

            if (HasFlags(m_Usage, ImageUsage::Sampled) && m_Aspect == (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))
            {
                ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                Check(vkCreateImageView(Ctx.Device(), &ViewInfo, nullptr, &m_SampledView), "vkCreateImageView (sampled)");
            }

            if (HasFlags(m_Usage, ImageUsage::Sampled))
            {
                m_SampledIndex = Ctx.Bindless().Allocate(BindlessSlot::SampledImage);
                Ctx.Bindless().WriteSampledImage(m_SampledIndex, SampledView());
            }

            // Storage descriptors must cover a single mip level: multi-mip images expose storage per Subview(Mip).
            if (HasFlags(m_Usage, ImageUsage::Storage) && Desc.MipLevels == 1U)
            {
                m_StorageIndex = Ctx.Bindless().Allocate(BindlessSlot::StorageImage);
                Ctx.Bindless().WriteStorageImage(m_StorageIndex, m_View);
            }
        }
    }
    catch (...)
    {
        Reset();
        throw;
    }

    if (!std::empty(Desc.Name))
    {
        const std::string Name(Desc.Name);
        vmaSetAllocationName(Ctx.Allocator(), m_Allocation, Name.c_str());
        Ctx.SetDebugName(m_Image, VK_OBJECT_TYPE_IMAGE, Name);
        if (m_View != VK_NULL_HANDLE)
        {
            Ctx.SetDebugName(m_View, VK_OBJECT_TYPE_IMAGE_VIEW, Name);
        }
    }
}

void luvk::Image::NoteTransferUse(const std::uint64_t TransferValue) noexcept
{
    m_TransferUse = TransferValue > m_TransferUse ? TransferValue : m_TransferUse;
}

luvk::Image::~Image()
{
    Reset();
}

luvk::Image::Image(Image&& Other) noexcept
    : m_Context(std::exchange(Other.m_Context, nullptr)),
      m_Image(std::exchange(Other.m_Image, VK_NULL_HANDLE)),
      m_Allocation(std::exchange(Other.m_Allocation, nullptr)),
      m_View(std::exchange(Other.m_View, VK_NULL_HANDLE)),
      m_SampledView(std::exchange(Other.m_SampledView, VK_NULL_HANDLE)),
      m_Subviews(std::move(Other.m_Subviews)),
      m_Extent(Other.m_Extent),
      m_Format(Other.m_Format),
      m_Usage(Other.m_Usage),
      m_Aspect(Other.m_Aspect),
      m_MipLevels(Other.m_MipLevels),
      m_ArrayLayers(Other.m_ArrayLayers),
      m_SampledIndex(std::exchange(Other.m_SampledIndex, InvalidBindlessIndex)),
      m_StorageIndex(std::exchange(Other.m_StorageIndex, InvalidBindlessIndex)),
      m_TransferUse(std::exchange(Other.m_TransferUse, 0U)),
      m_Concurrent(Other.m_Concurrent),
      m_Cube(Other.m_Cube),
      m_State(std::exchange(Other.m_State, {})) {}

luvk::Image& luvk::Image::operator=(Image&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        m_Context      = std::exchange(Other.m_Context, nullptr);
        m_Image        = std::exchange(Other.m_Image, VK_NULL_HANDLE);
        m_Allocation   = std::exchange(Other.m_Allocation, nullptr);
        m_View         = std::exchange(Other.m_View, VK_NULL_HANDLE);
        m_SampledView  = std::exchange(Other.m_SampledView, VK_NULL_HANDLE);
        m_Subviews     = std::move(Other.m_Subviews);
        m_Extent       = Other.m_Extent;
        m_Format       = Other.m_Format;
        m_Usage        = Other.m_Usage;
        m_Aspect       = Other.m_Aspect;
        m_MipLevels    = Other.m_MipLevels;
        m_ArrayLayers  = Other.m_ArrayLayers;
        m_SampledIndex = std::exchange(Other.m_SampledIndex, InvalidBindlessIndex);
        m_StorageIndex = std::exchange(Other.m_StorageIndex, InvalidBindlessIndex);
        m_TransferUse  = std::exchange(Other.m_TransferUse, 0U);
        m_Concurrent   = Other.m_Concurrent;
        m_Cube         = Other.m_Cube;
        m_State        = std::exchange(Other.m_State, {});
    }
    return *this;
}

void luvk::Image::Reset() noexcept
{
    if (m_Context == nullptr)
    {
        return;
    }

    TimelinePoint Point = m_Context->CurrentRetirePoint();
    Point.Transfer      = m_TransferUse;
    m_TransferUse       = 0U;

    if (m_SampledIndex != InvalidBindlessIndex)
    {
        m_Context->Retire({.Kind = RetiredKind::BindlessSlot, .Slot = static_cast<std::uint32_t>(BindlessSlot::SampledImage), .Index = m_SampledIndex}, Point);
        m_SampledIndex = InvalidBindlessIndex;
    }

    if (m_StorageIndex != InvalidBindlessIndex)
    {
        m_Context->Retire({.Kind = RetiredKind::BindlessSlot, .Slot = static_cast<std::uint32_t>(BindlessSlot::StorageImage), .Index = m_StorageIndex}, Point);
        m_StorageIndex = InvalidBindlessIndex;
    }

    for (const std::unique_ptr<ImageSubview>& Subview : m_Subviews)
    {
        if (Subview->SampledIndex != InvalidBindlessIndex)
        {
            m_Context->Retire({.Kind = RetiredKind::BindlessSlot, .Slot = static_cast<std::uint32_t>(BindlessSlot::SampledImage), .Index = Subview->SampledIndex}, Point);
        }
        if (Subview->StorageIndex != InvalidBindlessIndex)
        {
            m_Context->Retire({.Kind = RetiredKind::BindlessSlot, .Slot = static_cast<std::uint32_t>(BindlessSlot::StorageImage), .Index = Subview->StorageIndex}, Point);
        }
        m_Context->Retire({.Kind = RetiredKind::ImageView, .Handle = HandleToU64(Subview->View)}, Point);
    }
    m_Subviews.clear();

    if (m_SampledView != VK_NULL_HANDLE)
    {
        m_Context->Retire({.Kind = RetiredKind::ImageView, .Handle = HandleToU64(m_SampledView)}, Point);
        m_SampledView = VK_NULL_HANDLE;
    }

    if (m_View != VK_NULL_HANDLE)
    {
        m_Context->Retire({.Kind = RetiredKind::ImageView, .Handle = HandleToU64(m_View)}, Point);
        m_View = VK_NULL_HANDLE;
    }

    if (m_Image != VK_NULL_HANDLE)
    {
        m_Context->Retire({.Kind = RetiredKind::Image, .Handle = HandleToU64(m_Image), .Allocation = m_Allocation}, Point);
        m_Context->TrackResource(-1);
        m_Image      = VK_NULL_HANDLE;
        m_Allocation = nullptr;
    }

    m_State = {};
}

const luvk::ImageSubview& luvk::Image::Subview(const std::uint32_t MipLevel, const std::uint32_t ArrayLayer)
{
    if (m_View == VK_NULL_HANDLE)
    {
        throw std::logic_error("[luvk] Subviews need an image with a view-compatible usage.");
    }

    if ((MipLevel != AllSubresources && MipLevel >= m_MipLevels) || (ArrayLayer != AllSubresources && ArrayLayer >= m_ArrayLayers))
    {
        throw std::out_of_range("[luvk] Image subview: mip level or array layer out of range.");
    }

    for (const std::unique_ptr<ImageSubview>& Existing : m_Subviews)
    {
        if (Existing->MipLevel == MipLevel && Existing->ArrayLayer == ArrayLayer)
        {
            return *Existing;
        }
    }

    const bool OneLayer = ArrayLayer != AllSubresources || m_ArrayLayers == 1U;
    const bool OneMip   = MipLevel != AllSubresources || m_MipLevels == 1U;

    VkImageViewType Type = VK_IMAGE_VIEW_TYPE_2D;
    if (m_Extent.Depth > 1U)
    {
        Type = VK_IMAGE_VIEW_TYPE_3D;
    }
    else if (!OneLayer)
    {
        Type = m_Cube ? (m_ArrayLayers > 6U ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY : VK_IMAGE_VIEW_TYPE_CUBE) : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    }

    auto Created        = std::make_unique<ImageSubview>(ImageSubview{.MipLevel = MipLevel, .ArrayLayer = ArrayLayer});
    const VkImageViewCreateInfo ViewInfo{.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                         .image            = m_Image,
                                         .viewType         = Type,
                                         .format           = ToVk(m_Format),
                                         .subresourceRange = {m_Aspect,
                                                              MipLevel == AllSubresources ? 0U : MipLevel,
                                                              MipLevel == AllSubresources ? VK_REMAINING_MIP_LEVELS : 1U,
                                                              ArrayLayer == AllSubresources ? 0U : ArrayLayer,
                                                              ArrayLayer == AllSubresources ? VK_REMAINING_ARRAY_LAYERS : 1U}};
    Check(vkCreateImageView(m_Context->Device(), &ViewInfo, nullptr, &Created->View), "vkCreateImageView (subview)");

    // Combined depth/stencil views cannot be sampled; storage descriptors must cover a single 2D/3D mip level.
    const bool Sampleable = HasFlags(m_Usage, ImageUsage::Sampled) && m_Aspect != (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
    const bool Storable   = HasFlags(m_Usage, ImageUsage::Storage) && OneMip && (OneLayer || m_Extent.Depth > 1U);

    if (Sampleable)
    {
        Created->SampledIndex = m_Context->Bindless().Allocate(BindlessSlot::SampledImage);
        m_Context->Bindless().WriteSampledImage(Created->SampledIndex, Created->View);
    }

    if (Storable)
    {
        Created->StorageIndex = m_Context->Bindless().Allocate(BindlessSlot::StorageImage);
        m_Context->Bindless().WriteStorageImage(Created->StorageIndex, Created->View);
    }

    return *m_Subviews.emplace_back(std::move(Created));
}

bool luvk::Image::IsSharedWithTransferQueue() const noexcept
{
    return m_Concurrent || (m_Context != nullptr && !m_Context->HasDedicatedTransferQueue());
}

bool luvk::Image::IsSharedWithComputeQueue() const noexcept
{
    return m_Concurrent || (m_Context != nullptr && (!m_Context->HasAsyncCompute() || m_Context->ComputeQueue().Family() == m_Context->GraphicsQueue().Family()));
}

void luvk::Image::Upload(const std::span<const std::byte> Data, const std::uint32_t MipLevel, const std::uint32_t ArrayLayer)
{
    Upload(Data, ImageRegion{.MipLevel = MipLevel, .ArrayLayer = ArrayLayer});
}

void luvk::Image::Upload(const std::span<const std::byte> Data, const ImageRegion& Region)
{
    if (m_Image == VK_NULL_HANDLE)
    {
        throw std::logic_error("[luvk] Upload into an empty Image.");
    }

    m_Context->Uploads().Upload(*this, Data, Region);
}

luvk::Texture2D luvk::Image::Sampled() const
{
    if (Is3D() || m_Cube || m_ArrayLayers > 1U)
    {
        throw std::logic_error("[luvk] Image::Sampled is for 2D images: use Sampled3D, SampledCube or SampledArray.");
    }
    return {m_SampledIndex};
}

luvk::Texture3D luvk::Image::Sampled3D() const
{
    if (!Is3D())
    {
        throw std::logic_error("[luvk] Image::Sampled3D needs a 3D image.");
    }
    return {m_SampledIndex};
}

luvk::TextureCube luvk::Image::SampledCube() const
{
    if (!m_Cube)
    {
        throw std::logic_error("[luvk] Image::SampledCube needs a cube image.");
    }
    return {m_SampledIndex};
}

luvk::TextureArray luvk::Image::SampledArray() const
{
    if (Is3D() || m_Cube || m_ArrayLayers == 1U)
    {
        throw std::logic_error("[luvk] Image::SampledArray needs a 2D array image.");
    }
    return {m_SampledIndex};
}
