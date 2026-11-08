// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <type_traits>

static_assert(sizeof(void*) == 8U, "luvk targets 64-bit platforms only (Vulkan non-dispatchable handles are then pointers)");

/**
 * Opaque handles. These are the exact typedefs of vulkan_core.h and vk_mem_alloc.h on 64-bit platforms (a repeated typedef
 * of the same type is valid C++), so the escape-hatch accessors (Handle(), View(), ...) are declared without the Vulkan
 * headers and remain the real Vulkan types for code that includes luvk/Core/Vulkan.hpp.
 */
typedef struct VkInstance_T*                 VkInstance;
typedef struct VkPhysicalDevice_T*           VkPhysicalDevice;
typedef struct VkDevice_T*                   VkDevice;
typedef struct VkQueue_T*                    VkQueue;
typedef struct VkSemaphore_T*                VkSemaphore;
typedef struct VkFence_T*                    VkFence;
typedef struct VkCommandBuffer_T*            VkCommandBuffer;
typedef struct VkCommandPool_T*              VkCommandPool;
typedef struct VkBuffer_T*                   VkBuffer;
typedef struct VkImage_T*                    VkImage;
typedef struct VkImageView_T*                VkImageView;
typedef struct VkSampler_T*                  VkSampler;
typedef struct VkPipeline_T*                 VkPipeline;
typedef struct VkPipelineLayout_T*           VkPipelineLayout;
typedef struct VkPipelineCache_T*            VkPipelineCache;
typedef struct VkDescriptorSetLayout_T*      VkDescriptorSetLayout;
typedef struct VkDescriptorPool_T*           VkDescriptorPool;
typedef struct VkDescriptorSet_T*            VkDescriptorSet;
typedef struct VkQueryPool_T*                VkQueryPool;
typedef struct VkSurfaceKHR_T*               VkSurfaceKHR;
typedef struct VkSwapchainKHR_T*             VkSwapchainKHR;
typedef struct VkAccelerationStructureKHR_T* VkAccelerationStructureKHR;
typedef struct VkDebugUtilsMessengerEXT_T*   VkDebugUtilsMessengerEXT;
typedef struct VmaAllocator_T*               VmaAllocator;
typedef struct VmaAllocation_T*              VmaAllocation;

namespace luvk
{
    /** Size or offset in bytes of GPU memory. */
    using DeviceSize = std::uint64_t;

    /** Buffer device address (a GPU pointer). */
    using DeviceAddress = std::uint64_t;

    /** "Up to the end of the resource" for sizes. */
    inline constexpr DeviceSize WholeSize = ~0ULL;

    /** Upper LOD clamp that disables clamping. */
    inline constexpr float LodClampNone = 1000.F;

    /**
     * Enables the bitwise operators on a flag enumeration: specialise with `template <> struct EnableFlags<MyFlags> :
     * std::true_type {};`.
     */
    template <typename T>
    struct EnableFlags : std::false_type
    {
    };

    /** Bitwise or of two flag sets. */
    template <typename T>
        requires EnableFlags<T>::value
    [[nodiscard]] constexpr T operator|(const T Left, const T Right) noexcept
    {
        using U = std::underlying_type_t<T>;
        return static_cast<T>(static_cast<U>(Left) | static_cast<U>(Right));
    }

    /** Bitwise and of two flag sets. */
    template <typename T>
        requires EnableFlags<T>::value
    [[nodiscard]] constexpr T operator&(const T Left, const T Right) noexcept
    {
        using U = std::underlying_type_t<T>;
        return static_cast<T>(static_cast<U>(Left) & static_cast<U>(Right));
    }

    /** Complement of a flag set. */
    template <typename T>
        requires EnableFlags<T>::value
    [[nodiscard]] constexpr T operator~(const T Value) noexcept
    {
        using U = std::underlying_type_t<T>;
        return static_cast<T>(~static_cast<U>(Value));
    }

    /** Adds Right to Left. */
    template <typename T>
        requires EnableFlags<T>::value
    constexpr T& operator|=(T& Left, const T Right) noexcept
    {
        return Left = Left | Right;
    }

    /** True when every bit of Wanted is set in Value. */
    template <typename T>
        requires EnableFlags<T>::value
    [[nodiscard]] constexpr bool HasFlags(const T Value, const T Wanted) noexcept
    {
        return (Value & Wanted) == Wanted;
    }

    /** True when any bit of Wanted is set in Value. */
    template <typename T>
        requires EnableFlags<T>::value
    [[nodiscard]] constexpr bool HasAnyFlag(const T Value, const T Wanted) noexcept
    {
        using U = std::underlying_type_t<T>;
        return static_cast<U>(Value & Wanted) != 0U;
    }

    /** Texel format. The values are the VkFormat values, so conversions are free. */
    enum class Format : std::uint32_t
    {
        Undefined               = 0U,
        R4G4B4A4UnormPack16     = 2U,
        B4G4R4A4UnormPack16     = 3U,
        R5G6B5UnormPack16       = 4U,
        B5G6R5UnormPack16       = 5U,
        R8Unorm                 = 9U,
        R8Snorm                 = 10U,
        R8Uint                  = 13U,
        R8Sint                  = 14U,
        R8Srgb                  = 15U,
        R8G8Unorm               = 16U,
        R8G8Snorm               = 17U,
        R8G8Uint                = 20U,
        R8G8Sint                = 21U,
        R8G8B8A8Unorm           = 37U,
        R8G8B8A8Snorm           = 38U,
        R8G8B8A8Uint            = 41U,
        R8G8B8A8Sint            = 42U,
        R8G8B8A8Srgb            = 43U,
        B8G8R8A8Unorm           = 44U,
        B8G8R8A8Srgb            = 50U,
        A2R10G10B10UnormPack32  = 58U,
        A2B10G10R10UnormPack32  = 64U,
        R16Unorm                = 70U,
        R16Snorm                = 71U,
        R16Uint                 = 74U,
        R16Sint                 = 75U,
        R16Sfloat               = 76U,
        R16G16Unorm             = 77U,
        R16G16Snorm             = 78U,
        R16G16Uint              = 81U,
        R16G16Sint              = 82U,
        R16G16Sfloat            = 83U,
        R16G16B16A16Unorm       = 91U,
        R16G16B16A16Snorm       = 92U,
        R16G16B16A16Uint        = 95U,
        R16G16B16A16Sint        = 96U,
        R16G16B16A16Sfloat      = 97U,
        R32Uint                 = 98U,
        R32Sint                 = 99U,
        R32Sfloat               = 100U,
        R32G32Uint              = 101U,
        R32G32Sint              = 102U,
        R32G32Sfloat            = 103U,
        R32G32B32Uint           = 104U,
        R32G32B32Sint           = 105U,
        R32G32B32Sfloat         = 106U,
        R32G32B32A32Uint        = 107U,
        R32G32B32A32Sint        = 108U,
        R32G32B32A32Sfloat      = 109U,
        B10G11R11UfloatPack32   = 122U,
        E5B9G9R9UfloatPack32    = 123U,
        D16Unorm                = 124U,
        X8D24UnormPack32        = 125U,
        D32Sfloat               = 126U,
        S8Uint                  = 127U,
        D16UnormS8Uint          = 128U,
        D24UnormS8Uint          = 129U,
        D32SfloatS8Uint         = 130U,
        Bc1RgbUnorm             = 131U,
        Bc1RgbSrgb              = 132U,
        Bc1RgbaUnorm            = 133U,
        Bc1RgbaSrgb             = 134U,
        Bc2Unorm                = 135U,
        Bc2Srgb                 = 136U,
        Bc3Unorm                = 137U,
        Bc3Srgb                 = 138U,
        Bc4Unorm                = 139U,
        Bc4Snorm                = 140U,
        Bc5Unorm                = 141U,
        Bc5Snorm                = 142U,
        Bc6hUfloat              = 143U,
        Bc6hSfloat              = 144U,
        Bc7Unorm                = 145U,
        Bc7Srgb                 = 146U,
    };

    /**
     * Image usage. Optional on ImageDesc: Auto derives it from the format (sampled, transfer, storage when the format
     * supports it, color or depth attachment); an explicit set restricts the image to it (transfer is always added).
     */
    enum class ImageUsage : std::uint32_t
    {
        Auto                   = 0U,
        TransferSrc            = 0x1U,
        TransferDst            = 0x2U,
        Sampled                = 0x4U,
        Storage                = 0x8U,
        ColorAttachment        = 0x10U,
        DepthStencilAttachment = 0x20U,
    };

    template <>
    struct EnableFlags<ImageUsage> : std::true_type
    {
    };

    /** Rasterization sample count. */
    enum class SampleCount : std::uint32_t
    {
        X1  = 0x1U,
        X2  = 0x2U,
        X4  = 0x4U,
        X8  = 0x8U,
        X16 = 0x10U,
    };

    /** Depth, stencil and sampler comparison. */
    enum class CompareOp : std::uint32_t
    {
        Never          = 0U,
        Less           = 1U,
        Equal          = 2U,
        LessOrEqual    = 3U,
        Greater        = 4U,
        NotEqual       = 5U,
        GreaterOrEqual = 6U,
        Always         = 7U,
    };

    /** Texture filter. */
    enum class Filter : std::uint32_t
    {
        Nearest = 0U,
        Linear  = 1U,
    };

    /** Filter between mip levels. */
    enum class MipmapMode : std::uint32_t
    {
        Nearest = 0U,
        Linear  = 1U,
    };

    /** Texture coordinate wrapping. */
    enum class AddressMode : std::uint32_t
    {
        Repeat            = 0U,
        MirroredRepeat    = 1U,
        ClampToEdge       = 2U,
        ClampToBorder     = 3U,
        MirrorClampToEdge = 4U,
    };

    /** Border color of AddressMode::ClampToBorder. */
    enum class BorderColor : std::uint32_t
    {
        FloatTransparentBlack = 0U,
        IntTransparentBlack   = 1U,
        FloatOpaqueBlack      = 2U,
        IntOpaqueBlack        = 3U,
        FloatOpaqueWhite      = 4U,
        IntOpaqueWhite        = 5U,
    };

    /** How a sampler combines the filtered texels. */
    enum class ReductionMode : std::uint32_t
    {
        WeightedAverage = 0U,
        Min             = 1U,
        Max             = 2U,
    };

    /** Index buffer element type. */
    enum class IndexType : std::uint32_t
    {
        UInt16 = 0U,
        UInt32 = 1U,
    };

    /** Input assembly primitive topology. */
    enum class Topology : std::uint32_t
    {
        PointList     = 0U,
        LineList      = 1U,
        LineStrip     = 2U,
        TriangleList  = 3U,
        TriangleStrip = 4U,
        TriangleFan   = 5U,
    };

    /** Faces culled by the rasterizer. */
    enum class CullMode : std::uint32_t
    {
        None         = 0U,
        Front        = 0x1U,
        Back         = 0x2U,
        FrontAndBack = 0x3U,
    };

    /** Winding order considered front-facing. */
    enum class FrontFace : std::uint32_t
    {
        CounterClockwise = 0U,
        Clockwise        = 1U,
    };

    /** Rasterization polygon mode. */
    enum class PolygonMode : std::uint32_t
    {
        Fill  = 0U,
        Line  = 1U,
        Point = 2U,
    };

    /** Stencil operation. */
    enum class StencilOp : std::uint32_t
    {
        Keep              = 0U,
        Zero              = 1U,
        Replace           = 2U,
        IncrementAndClamp = 3U,
        DecrementAndClamp = 4U,
        Invert            = 5U,
        IncrementAndWrap  = 6U,
        DecrementAndWrap  = 7U,
    };

    /** Stencil faces a dynamic state applies to. */
    enum class StencilFace : std::uint32_t
    {
        Front        = 0x1U,
        Back         = 0x2U,
        FrontAndBack = 0x3U,
    };

    /** Vertex input rate of a vertex binding. */
    enum class VertexInputRate : std::uint32_t
    {
        Vertex   = 0U,
        Instance = 1U,
    };

    /** Presentation mode of a swapchain. */
    enum class PresentMode : std::uint32_t
    {
        Immediate   = 0U,
        Mailbox     = 1U,
        Fifo        = 2U,
        FifoRelaxed = 3U,
    };

    /** Color space of a swapchain (other VkColorSpaceKHR values may be cast in). */
    enum class ColorSpace : std::uint32_t
    {
        SrgbNonlinear = 0U,
    };

    /** Alpha compositing of a swapchain with the desktop. */
    enum class CompositeAlpha : std::uint32_t
    {
        Opaque         = 0x1U,
        PreMultiplied  = 0x2U,
        PostMultiplied = 0x4U,
        Inherit        = 0x8U,
    };

    /** Width and height in texels. */
    struct Extent2D
    {
        /** Width. */
        std::uint32_t Width = 0U;

        /** Height. */
        std::uint32_t Height = 0U;

        /** Component-wise equality. */
        [[nodiscard]] friend constexpr bool operator==(const Extent2D&, const Extent2D&) noexcept = default;
    };

    /** Width, height and depth in texels. */
    struct Extent3D
    {
        /** Width. */
        std::uint32_t Width = 0U;

        /** Height. */
        std::uint32_t Height = 0U;

        /** Depth (1 for 2D images). */
        std::uint32_t Depth = 1U;

        /** Width and height. */
        [[nodiscard]] constexpr Extent2D To2D() const noexcept { return {Width, Height}; }

        /** Component-wise equality. */
        [[nodiscard]] friend constexpr bool operator==(const Extent3D&, const Extent3D&) noexcept = default;
    };

    /** Signed 2D texel offset. */
    struct Offset2D
    {
        /** X. */
        std::int32_t X = 0;

        /** Y. */
        std::int32_t Y = 0;
    };

    /** Signed 3D texel offset. */
    struct Offset3D
    {
        /** X. */
        std::int32_t X = 0;

        /** Y. */
        std::int32_t Y = 0;

        /** Z. */
        std::int32_t Z = 0;
    };

    /** Rectangle in texels. */
    struct Rect2D
    {
        /** Top-left corner. */
        Offset2D Offset = {};

        /** Size. */
        Extent2D Extent = {};
    };

    /** Viewport transform. */
    struct Viewport
    {
        /** Left edge. */
        float X = 0.F;

        /** Top edge. */
        float Y = 0.F;

        /** Width. */
        float Width = 0.F;

        /** Height. */
        float Height = 0.F;

        /** Depth mapped to the near plane. */
        float MinDepth = 0.F;

        /** Depth mapped to the far plane. */
        float MaxDepth = 1.F;
    };

    /** Clear value of a color attachment or image: float, signed or unsigned depending on the format. */
    union ClearColorValue
    {
        /** Float and normalized formats. */
        float Float[4];

        /** Signed integer formats. */
        std::int32_t Int[4];

        /** Unsigned integer formats. */
        std::uint32_t UInt[4];
    };

    /** Swapchain format and color space pair. */
    struct SurfaceFormat
    {
        /** Texel format. */
        luvk::Format Format = luvk::Format::Undefined;

        /** Color space. */
        luvk::ColorSpace ColorSpace = luvk::ColorSpace::SrgbNonlinear;
    };

    /** One vertex buffer binding of the vertex-input path. */
    struct VertexBinding
    {
        /** Binding slot. */
        std::uint32_t Binding = 0U;

        /** Bytes between consecutive elements. */
        std::uint32_t Stride = 0U;

        /** Per vertex or per instance. */
        VertexInputRate InputRate = VertexInputRate::Vertex;
    };

    /** One vertex attribute of the vertex-input path. */
    struct VertexAttribute
    {
        /** Shader input location. */
        std::uint32_t Location = 0U;

        /** Binding slot the attribute reads from. */
        std::uint32_t Binding = 0U;

        /** Attribute format. */
        luvk::Format Format = luvk::Format::Undefined;

        /** Byte offset inside the element. */
        std::uint32_t Offset = 0U;
    };

    /** Arguments of one DrawIndirect record (VkDrawIndirectCommand). */
    struct DrawIndirectArgs
    {
        /** Vertices per instance. */
        std::uint32_t VertexCount = 0U;

        /** Instances. */
        std::uint32_t InstanceCount = 0U;

        /** First vertex. */
        std::uint32_t FirstVertex = 0U;

        /** First instance. */
        std::uint32_t FirstInstance = 0U;
    };

    /** Arguments of one DrawIndexedIndirect record (VkDrawIndexedIndirectCommand). */
    struct DrawIndexedIndirectArgs
    {
        /** Indices per instance. */
        std::uint32_t IndexCount = 0U;

        /** Instances. */
        std::uint32_t InstanceCount = 0U;

        /** First index. */
        std::uint32_t FirstIndex = 0U;

        /** Value added to every index. */
        std::int32_t VertexOffset = 0;

        /** First instance. */
        std::uint32_t FirstInstance = 0U;
    };

    /** Arguments of one DrawMeshTasksIndirect record (VkDrawMeshTasksIndirectCommandEXT). */
    struct DrawMeshTasksIndirectArgs
    {
        /** Task (or mesh) workgroups on X. */
        std::uint32_t GroupCountX = 0U;

        /** Workgroups on Y. */
        std::uint32_t GroupCountY = 1U;

        /** Workgroups on Z. */
        std::uint32_t GroupCountZ = 1U;
    };

    /** Arguments of one DispatchIndirect record (VkDispatchIndirectCommand). */
    struct DispatchIndirectArgs
    {
        /** Workgroups on X. */
        std::uint32_t X = 0U;

        /** Workgroups on Y. */
        std::uint32_t Y = 1U;

        /** Workgroups on Z. */
        std::uint32_t Z = 1U;
    };

    /** Memory of one heap as reported by VK_EXT_memory_budget (through VMA). */
    struct HeapBudget
    {
        /** Bytes allocated by this process in the heap. */
        DeviceSize Usage = 0U;

        /** Bytes the process can allocate before the system starts paging. */
        DeviceSize Budget = 0U;

        /** True for device-local heaps. */
        bool DeviceLocal = false;
    };

    /** Device identity and the limits luvk users commonly need. */
    struct DeviceLimits
    {
        /** Marketing name of the GPU. */
        const char* DeviceName = "";

        /** PCI vendor identifier. */
        std::uint32_t VendorId = 0U;

        /** Vendor-specific driver version. */
        std::uint32_t DriverVersion = 0U;

        /** Device API version (packed major/minor/patch). */
        std::uint32_t ApiVersion = 0U;

        /** True for discrete GPUs. */
        bool Discrete = false;

        /** Nanoseconds per GPU timestamp tick. */
        float TimestampPeriod = 0.F;

        /** Largest 2D image side. */
        std::uint32_t MaxImageDimension2D = 0U;

        /** Largest 3D image side. */
        std::uint32_t MaxImageDimension3D = 0U;

        /** Maximum compute workgroups per dispatch on each axis. */
        std::uint32_t MaxComputeWorkGroupCount[3] = {};

        /** Maximum invocations of one compute workgroup. */
        std::uint32_t MaxComputeWorkGroupInvocations = 0U;

        /** Maximum sampler anisotropy. */
        float MaxSamplerAnisotropy = 1.F;

        /** Required alignment of storage buffer offsets. */
        DeviceSize MinStorageBufferOffsetAlignment = 0U;

        /** Subgroup (wave) size. */
        std::uint32_t SubgroupSize = 0U;
    };
}
