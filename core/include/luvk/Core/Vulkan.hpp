// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

/**
 * Interop header: the raw Vulkan and VMA declarations plus the conversions between the luvk types and their Vulkan
 * counterparts. Only code that talks to Vulkan directly (third-party integrations, custom modules) includes it; every
 * other luvk header compiles without the Vulkan headers on the include path.
 */

#include <bit>
#include <volk.h>

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Weverything"
#elif defined(_MSC_VER)
#    pragma warning(push, 0)
#elif defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wall"
#    pragma GCC diagnostic ignored "-Wextra"
#    pragma GCC diagnostic ignored "-Wpedantic"
#    pragma GCC diagnostic ignored "-Wunused-parameter"
#    pragma GCC diagnostic ignored "-Wunused-variable"
#    pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#include <vk_mem_alloc.h>

#if defined(__clang__)
#    pragma clang diagnostic pop
#elif defined(_MSC_VER)
#    pragma warning(pop)
#elif defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

#include "luvk/Core/Types.hpp"

namespace luvk
{
    static_assert(static_cast<VkFormat>(Format::R8G8B8A8Unorm) == VK_FORMAT_R8G8B8A8_UNORM);
    static_assert(static_cast<VkFormat>(Format::B8G8R8A8Srgb) == VK_FORMAT_B8G8R8A8_SRGB);
    static_assert(static_cast<VkFormat>(Format::A2B10G10R10UnormPack32) == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    static_assert(static_cast<VkFormat>(Format::R16G16B16A16Sfloat) == VK_FORMAT_R16G16B16A16_SFLOAT);
    static_assert(static_cast<VkFormat>(Format::R32G32B32A32Sfloat) == VK_FORMAT_R32G32B32A32_SFLOAT);
    static_assert(static_cast<VkFormat>(Format::B10G11R11UfloatPack32) == VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    static_assert(static_cast<VkFormat>(Format::D32SfloatS8Uint) == VK_FORMAT_D32_SFLOAT_S8_UINT);
    static_assert(static_cast<VkFormat>(Format::Bc7Srgb) == VK_FORMAT_BC7_SRGB_BLOCK);
    static_assert(static_cast<VkImageUsageFlags>(ImageUsage::DepthStencilAttachment) == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    static_assert(static_cast<VkSampleCountFlagBits>(SampleCount::X16) == VK_SAMPLE_COUNT_16_BIT);
    static_assert(static_cast<VkCompareOp>(CompareOp::GreaterOrEqual) == VK_COMPARE_OP_GREATER_OR_EQUAL);
    static_assert(static_cast<VkSamplerAddressMode>(AddressMode::MirrorClampToEdge) == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE);
    static_assert(static_cast<VkBorderColor>(BorderColor::IntOpaqueWhite) == VK_BORDER_COLOR_INT_OPAQUE_WHITE);
    static_assert(static_cast<VkSamplerReductionMode>(ReductionMode::Max) == VK_SAMPLER_REDUCTION_MODE_MAX);
    static_assert(static_cast<VkIndexType>(IndexType::UInt32) == VK_INDEX_TYPE_UINT32);
    static_assert(static_cast<VkPrimitiveTopology>(Topology::TriangleFan) == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN);
    static_assert(static_cast<VkCullModeFlags>(CullMode::FrontAndBack) == VK_CULL_MODE_FRONT_AND_BACK);
    static_assert(static_cast<VkFrontFace>(FrontFace::Clockwise) == VK_FRONT_FACE_CLOCKWISE);
    static_assert(static_cast<VkPolygonMode>(PolygonMode::Point) == VK_POLYGON_MODE_POINT);
    static_assert(static_cast<VkStencilOp>(StencilOp::DecrementAndWrap) == VK_STENCIL_OP_DECREMENT_AND_WRAP);
    static_assert(static_cast<VkStencilFaceFlags>(StencilFace::FrontAndBack) == VK_STENCIL_FACE_FRONT_AND_BACK);
    static_assert(static_cast<VkVertexInputRate>(VertexInputRate::Instance) == VK_VERTEX_INPUT_RATE_INSTANCE);
    static_assert(static_cast<VkPresentModeKHR>(PresentMode::FifoRelaxed) == VK_PRESENT_MODE_FIFO_RELAXED_KHR);
    static_assert(static_cast<VkCompositeAlphaFlagBitsKHR>(CompositeAlpha::Inherit) == VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR);
    static_assert(static_cast<VkFilter>(Filter::Linear) == VK_FILTER_LINEAR);
    static_assert(static_cast<VkSamplerMipmapMode>(MipmapMode::Linear) == VK_SAMPLER_MIPMAP_MODE_LINEAR);
    static_assert(sizeof(ClearColorValue) == sizeof(VkClearColorValue));
    static_assert(sizeof(Viewport) == sizeof(VkViewport) && sizeof(Rect2D) == sizeof(VkRect2D));
    static_assert(sizeof(Extent2D) == sizeof(VkExtent2D) && sizeof(Extent3D) == sizeof(VkExtent3D) && sizeof(Offset3D) == sizeof(VkOffset3D));
    static_assert(sizeof(DrawIndirectArgs) == sizeof(VkDrawIndirectCommand));
    static_assert(sizeof(DrawIndexedIndirectArgs) == sizeof(VkDrawIndexedIndirectCommand));
    static_assert(sizeof(DrawMeshTasksIndirectArgs) == sizeof(VkDrawMeshTasksIndirectCommandEXT));
    static_assert(sizeof(DispatchIndirectArgs) == sizeof(VkDispatchIndirectCommand));

    /** luvk format to VkFormat. */
    [[nodiscard]] constexpr VkFormat ToVk(const Format Value) noexcept { return static_cast<VkFormat>(Value); }

    /** VkFormat to the luvk format. */
    [[nodiscard]] constexpr Format FromVk(const VkFormat Value) noexcept { return static_cast<Format>(Value); }

    /** luvk image usage to VkImageUsageFlags. */
    [[nodiscard]] constexpr VkImageUsageFlags ToVk(const ImageUsage Value) noexcept { return static_cast<VkImageUsageFlags>(Value); }

    /** luvk sample count to VkSampleCountFlagBits. */
    [[nodiscard]] constexpr VkSampleCountFlagBits ToVk(const SampleCount Value) noexcept { return static_cast<VkSampleCountFlagBits>(Value); }

    /** luvk comparison to VkCompareOp. */
    [[nodiscard]] constexpr VkCompareOp ToVk(const CompareOp Value) noexcept { return static_cast<VkCompareOp>(Value); }

    /** luvk filter to VkFilter. */
    [[nodiscard]] constexpr VkFilter ToVk(const Filter Value) noexcept { return static_cast<VkFilter>(Value); }

    /** luvk mipmap mode to VkSamplerMipmapMode. */
    [[nodiscard]] constexpr VkSamplerMipmapMode ToVk(const MipmapMode Value) noexcept { return static_cast<VkSamplerMipmapMode>(Value); }

    /** luvk address mode to VkSamplerAddressMode. */
    [[nodiscard]] constexpr VkSamplerAddressMode ToVk(const AddressMode Value) noexcept { return static_cast<VkSamplerAddressMode>(Value); }

    /** luvk border color to VkBorderColor. */
    [[nodiscard]] constexpr VkBorderColor ToVk(const BorderColor Value) noexcept { return static_cast<VkBorderColor>(Value); }

    /** luvk reduction mode to VkSamplerReductionMode. */
    [[nodiscard]] constexpr VkSamplerReductionMode ToVk(const ReductionMode Value) noexcept { return static_cast<VkSamplerReductionMode>(Value); }

    /** luvk index type to VkIndexType. */
    [[nodiscard]] constexpr VkIndexType ToVk(const IndexType Value) noexcept { return static_cast<VkIndexType>(Value); }

    /** luvk topology to VkPrimitiveTopology. */
    [[nodiscard]] constexpr VkPrimitiveTopology ToVk(const Topology Value) noexcept { return static_cast<VkPrimitiveTopology>(Value); }

    /** luvk cull mode to VkCullModeFlags. */
    [[nodiscard]] constexpr VkCullModeFlags ToVk(const CullMode Value) noexcept { return static_cast<VkCullModeFlags>(Value); }

    /** luvk front face to VkFrontFace. */
    [[nodiscard]] constexpr VkFrontFace ToVk(const FrontFace Value) noexcept { return static_cast<VkFrontFace>(Value); }

    /** luvk polygon mode to VkPolygonMode. */
    [[nodiscard]] constexpr VkPolygonMode ToVk(const PolygonMode Value) noexcept { return static_cast<VkPolygonMode>(Value); }

    /** luvk stencil operation to VkStencilOp. */
    [[nodiscard]] constexpr VkStencilOp ToVk(const StencilOp Value) noexcept { return static_cast<VkStencilOp>(Value); }

    /** luvk stencil faces to VkStencilFaceFlags. */
    [[nodiscard]] constexpr VkStencilFaceFlags ToVk(const StencilFace Value) noexcept { return static_cast<VkStencilFaceFlags>(Value); }

    /** luvk vertex input rate to VkVertexInputRate. */
    [[nodiscard]] constexpr VkVertexInputRate ToVk(const VertexInputRate Value) noexcept { return static_cast<VkVertexInputRate>(Value); }

    /** luvk present mode to VkPresentModeKHR. */
    [[nodiscard]] constexpr VkPresentModeKHR ToVk(const PresentMode Value) noexcept { return static_cast<VkPresentModeKHR>(Value); }

    /** VkPresentModeKHR to the luvk present mode. */
    [[nodiscard]] constexpr PresentMode FromVk(const VkPresentModeKHR Value) noexcept { return static_cast<PresentMode>(Value); }

    /** luvk color space to VkColorSpaceKHR. */
    [[nodiscard]] constexpr VkColorSpaceKHR ToVk(const ColorSpace Value) noexcept { return static_cast<VkColorSpaceKHR>(Value); }

    /** luvk composite alpha to VkCompositeAlphaFlagBitsKHR. */
    [[nodiscard]] constexpr VkCompositeAlphaFlagBitsKHR ToVk(const CompositeAlpha Value) noexcept { return static_cast<VkCompositeAlphaFlagBitsKHR>(Value); }

    /** VkCompositeAlphaFlagBitsKHR to the luvk composite alpha. */
    [[nodiscard]] constexpr CompositeAlpha FromVk(const VkCompositeAlphaFlagBitsKHR Value) noexcept { return static_cast<CompositeAlpha>(Value); }

    /** luvk extent to VkExtent2D. */
    [[nodiscard]] constexpr VkExtent2D ToVk(const Extent2D Value) noexcept { return {Value.Width, Value.Height}; }

    /** VkExtent2D to the luvk extent. */
    [[nodiscard]] constexpr Extent2D FromVk(const VkExtent2D Value) noexcept { return {Value.width, Value.height}; }

    /** luvk extent to VkExtent3D. */
    [[nodiscard]] constexpr VkExtent3D ToVk(const Extent3D Value) noexcept { return {Value.Width, Value.Height, Value.Depth}; }

    /** VkExtent3D to the luvk extent. */
    [[nodiscard]] constexpr Extent3D FromVk(const VkExtent3D Value) noexcept { return {Value.width, Value.height, Value.depth}; }

    /** luvk offset to VkOffset3D. */
    [[nodiscard]] constexpr VkOffset3D ToVk(const Offset3D Value) noexcept { return {Value.X, Value.Y, Value.Z}; }

    /** luvk rectangle to VkRect2D. */
    [[nodiscard]] constexpr VkRect2D ToVk(const Rect2D Value) noexcept { return {{Value.Offset.X, Value.Offset.Y}, {Value.Extent.Width, Value.Extent.Height}}; }

    /** luvk viewport to VkViewport. */
    [[nodiscard]] constexpr VkViewport ToVk(const Viewport& Value) noexcept
    {
        return {Value.X, Value.Y, Value.Width, Value.Height, Value.MinDepth, Value.MaxDepth};
    }

    /** luvk clear color to VkClearColorValue (same 16 bytes). */
    [[nodiscard]] inline VkClearColorValue ToVk(const ClearColorValue& Value) noexcept { return std::bit_cast<VkClearColorValue>(Value); }

    /** luvk surface format to VkSurfaceFormatKHR. */
    [[nodiscard]] constexpr VkSurfaceFormatKHR ToVk(const SurfaceFormat Value) noexcept { return {ToVk(Value.Format), ToVk(Value.ColorSpace)}; }
}
