// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <bit>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    class Context;

    /** One 32-bit specialization constant (constant_id / [vk::constant_id]): bool, int, uint or float bits. */
    struct SpecializationConstant
    {
        /** constant_id declared in the shader. */
        std::uint32_t Id = 0U;

        /** Raw 32-bit value (VkBool32 for bools, the bit pattern for floats). */
        std::uint32_t Value = 0U;

        /** Boolean constant (stored as VkBool32). */
        [[nodiscard]] static constexpr SpecializationConstant Bool(const std::uint32_t ConstantId, const bool Enabled) noexcept
        {
            return {ConstantId, Enabled ? 1U : 0U};
        }

        /** Unsigned integer constant. */
        [[nodiscard]] static constexpr SpecializationConstant UInt(const std::uint32_t ConstantId, const std::uint32_t Number) noexcept
        {
            return {ConstantId, Number};
        }

        /** Signed integer constant. */
        [[nodiscard]] static constexpr SpecializationConstant Int(const std::uint32_t ConstantId, const std::int32_t Number) noexcept
        {
            return {ConstantId, std::bit_cast<std::uint32_t>(Number)};
        }

        /** 32-bit float constant. */
        [[nodiscard]] static constexpr SpecializationConstant Float(const std::uint32_t ConstantId, const float Number) noexcept
        {
            return {ConstantId, std::bit_cast<std::uint32_t>(Number)};
        }
    };

    /** A compiled shader stage: SPIR-V code with its entry point and optional specialization constants. */
    struct ShaderStage
    {
        /** SPIR-V words of the shader stage. */
        std::span<const std::uint32_t> Code;

        /** Entry point name inside the module. */
        std::string_view EntryPoint = "main";

        /** Specialization constants applied to this stage (ids must be unique). One SPIR-V module, many variants. */
        std::vector<SpecializationConstant> Constants;

        /** True when SPIR-V code was provided. */
        [[nodiscard]] constexpr bool IsSet() const noexcept { return !Code.empty(); }
    };

    /** Rasterizer depth bias (shadow maps, decals, coplanar overlays). */
    struct DepthBiasDesc
    {
        /** Enables depth bias. */
        bool Enable = false;

        /** Constant depth value added to each fragment. */
        float ConstantFactor = 0.F;

        /** Factor applied to the fragment's depth slope. */
        float SlopeFactor = 0.F;

        /** Maximum (or minimum, when negative) bias; non-zero values require the depthBiasClamp device feature. */
        float Clamp = 0.F;

        /** The factors are set per draw with CommandList::SetDepthBias (VK_DYNAMIC_STATE_DEPTH_BIAS) instead of the values above. */
        bool Dynamic = false;
    };

    /** Stencil operations of one face. */
    struct StencilFaceDesc
    {
        /** Operation when the stencil test fails. */
        VkStencilOp Fail = VK_STENCIL_OP_KEEP;

        /** Operation when both the stencil and the depth tests pass. */
        VkStencilOp Pass = VK_STENCIL_OP_KEEP;

        /** Operation when the stencil test passes and the depth test fails. */
        VkStencilOp DepthFail = VK_STENCIL_OP_KEEP;

        /** Stencil comparison against the reference. */
        VkCompareOp Compare = VK_COMPARE_OP_ALWAYS;

        /** Bits of the reference and stored value that take part in the comparison. */
        std::uint32_t CompareMask = 0xFFU;

        /** Bits written by the stencil operations. */
        std::uint32_t WriteMask = 0xFFU;
    };

    /** Stencil test (requires a DepthFormat with a stencil aspect, e.g. VK_FORMAT_D32_SFLOAT_S8_UINT). */
    struct StencilDesc
    {
        /** Enables the stencil test. */
        bool Enable = false;

        /** Operations of front-facing primitives. */
        StencilFaceDesc Front;

        /** Operations of back-facing primitives. */
        StencilFaceDesc Back;

        /** Reference value used by both faces. */
        std::uint32_t Reference = 0U;

        /** The reference is set per draw with CommandList::SetStencilReference (VK_DYNAMIC_STATE_STENCIL_REFERENCE). */
        bool DynamicReference = false;
    };

    /** Color blending preset applied to each color attachment. */
    enum class BlendMode : std::uint8_t
    {
        Opaque,
        Alpha,
        PremultipliedAlpha,
        Additive,
    };

    /** Graphics pipeline built for dynamic rendering (no VkRenderPass / VkFramebuffer), with dynamic viewport and scissor
     *  set by the render graph to the attachment size. */
    struct GraphicsPipelineDesc
    {
        /** Classic pipeline: vertex (+ fragment). */
        ShaderStage Vertex;

        /** Mesh pipeline: optional task + mesh (+ fragment). Requires MeshShaderModule. */
        ShaderStage Task;

        /** Mesh shader stage (mutually exclusive with Vertex; required when Task is set). */
        ShaderStage Mesh;

        /** Fragment shader stage (optional, e.g. depth-only passes). */
        ShaderStage Fragment;

        /** Color attachment formats, in draw order. */
        std::vector<VkFormat> ColorFormats;

        /** Per color attachment; missing entries are Opaque. */
        std::vector<BlendMode> Blend;

        /** Depth/stencil attachment format, or VK_FORMAT_UNDEFINED for none. */
        VkFormat DepthFormat = VK_FORMAT_UNDEFINED;

        /** Empty = vertex pulling through buffer addresses. */
        std::vector<VkVertexInputBindingDescription> VertexBindings;

        /** Vertex input attribute descriptions matching VertexBindings. */
        std::vector<VkVertexInputAttributeDescription> VertexAttributes;

        /** Input assembly primitive topology. */
        VkPrimitiveTopology Topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        /** Rasterization polygon mode. */
        VkPolygonMode PolygonMode = VK_POLYGON_MODE_FILL;

        /** Face culling mode. */
        VkCullModeFlags CullMode = VK_CULL_MODE_BACK_BIT;

        /** Winding order considered front-facing. */
        VkFrontFace FrontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

        /** Enable depth testing (requires a DepthFormat). */
        bool DepthTest = true;

        /** Enable depth writes. */
        bool DepthWrite = true;

        /** Depth comparison operator. */
        VkCompareOp DepthCompare = VK_COMPARE_OP_LESS_OR_EQUAL;

        /** Rasterizer depth bias (disabled by default). */
        DepthBiasDesc DepthBias;

        /** Stencil test (disabled by default). */
        StencilDesc Stencil;

        /** Rasterization sample count. */
        VkSampleCountFlagBits Samples = VK_SAMPLE_COUNT_1_BIT;

        /** Optional debug name for the pipeline. */
        std::string_view Name;
    };

    /** Creation parameters for a compute luvk::Pipeline. */
    struct ComputePipelineDesc
    {
        /** The compute shader stage. */
        ShaderStage Compute;

        /** Optional debug name for the pipeline. */
        std::string_view Name;
    };

    /** Move-only RAII pipeline. Every pipeline uses the global bindless layout (Context::Bindless()), so there is nothing to
     *  configure about descriptor sets: resources are indices / buffer addresses passed via push constants (up to luvk::PushConstantSize bytes, visible to all stages). */
    class LUVK_API Pipeline
    {
        /** Owning Context used for the bindless layout and retirement. */
        Context* m_Context = nullptr;

        /** Vulkan pipeline handle. */
        VkPipeline m_Pipeline = VK_NULL_HANDLE;

        /** Graphics or compute bind point. */
        VkPipelineBindPoint m_BindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;

    public:
        /** Default constructor. */
        Pipeline() = default;

        /** Constructor. Builds a graphics pipeline for dynamic rendering: vertex or task + mesh stages plus optional fragment. */
        Pipeline(Context& Ctx, const GraphicsPipelineDesc& Desc);

        /** Constructor. Builds a compute pipeline. */
        Pipeline(Context& Ctx, const ComputePipelineDesc& Desc);

        /** Destructor. */
        ~Pipeline();

        /** Non-copyable. */
        Pipeline(const Pipeline&)            = delete;

        /** Non-copyable. */
        Pipeline& operator=(const Pipeline&) = delete;

        /** Move-only. */
        Pipeline(Pipeline&& Other) noexcept;

        /** Move-only. */
        Pipeline& operator=(Pipeline&& Other) noexcept;

        /** Releases the pipeline through the deletion queue: safe to call (or to reassign, e.g. on shader hot reload) while frames that use it are still in flight. */
        void Reset() noexcept;

        /** True when a VkPipeline was created. */
        [[nodiscard]] explicit operator bool() const noexcept { return m_Pipeline != VK_NULL_HANDLE; }

        /** Underlying VkPipeline handle. */
        [[nodiscard]] VkPipeline          Handle() const noexcept { return m_Pipeline; }

        /** Graphics or compute bind point used with vkCmdBindPipeline. */
        [[nodiscard]] VkPipelineBindPoint BindPoint() const noexcept { return m_BindPoint; }

        /** Global bindless pipeline layout (Context::Bindless()), or VK_NULL_HANDLE when the pipeline is empty. */
        [[nodiscard]] VkPipelineLayout    Layout() const noexcept;
    };
}
