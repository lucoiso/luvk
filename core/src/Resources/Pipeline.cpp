// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "PipelineInternals.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include <array>
#include <stdexcept>
#include <string>
#include <utility>
#include "luvk/Core/Bindless.hpp"
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Resources/Image.hpp"

namespace
{

    VkPipelineColorBlendAttachmentState MakeBlend(const luvk::BlendMode Mode)
    {
        VkPipelineColorBlendAttachmentState State{.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                    VK_COLOR_COMPONENT_G_BIT |
                                                                    VK_COLOR_COMPONENT_B_BIT |
                                                                    VK_COLOR_COMPONENT_A_BIT};
        switch (Mode)
        {
        case luvk::BlendMode::Opaque:
            break;
        case luvk::BlendMode::Alpha:
            State.blendEnable         = VK_TRUE;
            State.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            State.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            State.colorBlendOp        = VK_BLEND_OP_ADD;
            State.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            State.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            State.alphaBlendOp        = VK_BLEND_OP_ADD;
            break;
        case luvk::BlendMode::PremultipliedAlpha:
            State.blendEnable         = VK_TRUE;
            State.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            State.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            State.colorBlendOp        = VK_BLEND_OP_ADD;
            State.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            State.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            State.alphaBlendOp        = VK_BLEND_OP_ADD;
            break;
        case luvk::BlendMode::Additive:
            State.blendEnable         = VK_TRUE;
            State.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            State.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            State.colorBlendOp        = VK_BLEND_OP_ADD;
            State.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            State.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            State.alphaBlendOp        = VK_BLEND_OP_ADD;
            break;
        }
        return State;
    }

    VkStencilOpState ToVulkan(const luvk::StencilFaceDesc& Face, const std::uint32_t Reference) noexcept
    {
        return VkStencilOpState{.failOp      = Face.Fail,
                                .passOp      = Face.Pass,
                                .depthFailOp = Face.DepthFail,
                                .compareOp   = Face.Compare,
                                .compareMask = Face.CompareMask,
                                .writeMask   = Face.WriteMask,
                                .reference   = Reference};
    }
}

luvk::Pipeline::Pipeline(Context& Ctx, const GraphicsPipelineDesc& Desc)
    : m_Context(&Ctx),
      m_BindPoint(VK_PIPELINE_BIND_POINT_GRAPHICS)
{
    const bool IsMeshPipeline = Desc.Mesh.IsSet();

    if (IsMeshPipeline == Desc.Vertex.IsSet())
    {
        throw std::invalid_argument("[luvk] A graphics pipeline needs either a vertex shader or a mesh shader (not both).");
    }

    if (Desc.Task.IsSet() && !IsMeshPipeline)
    {
        throw std::invalid_argument("[luvk] A task shader requires a mesh shader.");
    }

    if (IsMeshPipeline)
    {
        const auto* const MeshFeatures = Ctx.EnabledFeatures().Find<VkPhysicalDeviceMeshShaderFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT);
        if (MeshFeatures == nullptr || MeshFeatures->meshShader == VK_FALSE || !Ctx.IsDeviceExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME))
        {
            throw std::logic_error("[luvk] Mesh pipelines require luvk::MeshShaderModule (VK_EXT_mesh_shader).");
        }
    }

    if (std::empty(Desc.ColorFormats) && Desc.DepthFormat == VK_FORMAT_UNDEFINED)
    {
        throw std::invalid_argument("[luvk] A graphics pipeline needs at least one color or depth format.");
    }

    if (Desc.Stencil.Enable && !HasStencil(Desc.DepthFormat))
    {
        throw std::invalid_argument("[luvk] The stencil test needs a DepthFormat with a stencil aspect (e.g. VK_FORMAT_D32_SFLOAT_S8_UINT).");
    }

    if (Desc.DepthBias.Enable && !Desc.DepthBias.Dynamic && Desc.DepthBias.Clamp != 0.F && Ctx.EnabledFeatures().Core().depthBiasClamp == VK_FALSE)
    {
        throw std::logic_error("[luvk] A depth bias clamp requires the depthBiasClamp device feature.");
    }

    const VkDevice Device = Ctx.Device();

    StageBuilder                       Builder;
    std::vector<VkShaderStageFlagBits> Flags;
    auto                               AddStage = [&](const ShaderStage& Stage, const VkShaderStageFlagBits Flag)
    {
        if (Stage.IsSet())
        {
            Builder.Add(Device, Stage);
            Flags.push_back(Flag);
        }
    };

    AddStage(Desc.Vertex, VK_SHADER_STAGE_VERTEX_BIT);
    AddStage(Desc.Task, VK_SHADER_STAGE_TASK_BIT_EXT);
    AddStage(Desc.Mesh, VK_SHADER_STAGE_MESH_BIT_EXT);
    AddStage(Desc.Fragment, VK_SHADER_STAGE_FRAGMENT_BIT);
    Builder.Finalize(Flags);

    const VkPipelineVertexInputStateCreateInfo VertexInput{.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                           .vertexBindingDescriptionCount   = static_cast<std::uint32_t>(std::size(Desc.VertexBindings)),
                                                           .pVertexBindingDescriptions      = std::data(Desc.VertexBindings),
                                                           .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(std::size(Desc.VertexAttributes)),
                                                           .pVertexAttributeDescriptions    = std::data(Desc.VertexAttributes)};

    const VkPipelineInputAssemblyStateCreateInfo InputAssembly{.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                               .topology = Desc.Topology};

    constexpr VkPipelineViewportStateCreateInfo ViewportState{.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                              .viewportCount = 1U,
                                                              .scissorCount  = 1U};

    const VkPipelineRasterizationStateCreateInfo Rasterization{.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                               .polygonMode             = Desc.PolygonMode,
                                                               .cullMode                = Desc.CullMode,
                                                               .frontFace               = Desc.FrontFace,
                                                               .depthBiasEnable         = Desc.DepthBias.Enable ? VK_TRUE : VK_FALSE,
                                                               .depthBiasConstantFactor = Desc.DepthBias.ConstantFactor,
                                                               .depthBiasClamp          = Desc.DepthBias.Clamp,
                                                               .depthBiasSlopeFactor    = Desc.DepthBias.SlopeFactor,
                                                               .lineWidth               = 1.F};

    const VkPipelineMultisampleStateCreateInfo Multisample{.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                           .rasterizationSamples = Desc.Samples};

    const bool HasDepth = Desc.DepthFormat != VK_FORMAT_UNDEFINED;

    const VkPipelineDepthStencilStateCreateInfo DepthStencil{.sType             = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                             .depthTestEnable   = HasDepth && Desc.DepthTest ? VK_TRUE : VK_FALSE,
                                                             .depthWriteEnable  = HasDepth && Desc.DepthWrite ? VK_TRUE : VK_FALSE,
                                                             .depthCompareOp    = Desc.DepthCompare,
                                                             .stencilTestEnable = Desc.Stencil.Enable ? VK_TRUE : VK_FALSE,
                                                             .front             = ToVulkan(Desc.Stencil.Front, Desc.Stencil.Reference),
                                                             .back              = ToVulkan(Desc.Stencil.Back, Desc.Stencil.Reference),
                                                             .maxDepthBounds    = 1.F};

    std::vector<VkPipelineColorBlendAttachmentState> BlendStates;
    BlendStates.reserve(std::size(Desc.ColorFormats));
    for (std::size_t Index = 0U; Index < std::size(Desc.ColorFormats); ++Index)
    {
        BlendStates.push_back(MakeBlend(Index < std::size(Desc.Blend) ? Desc.Blend[Index] : BlendMode::Opaque));
    }

    const VkPipelineColorBlendStateCreateInfo ColorBlend{.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                         .attachmentCount = static_cast<std::uint32_t>(std::size(BlendStates)),
                                                         .pAttachments    = std::data(BlendStates)};

    std::array<VkDynamicState, 4> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    std::uint32_t                 DynamicCount  = 2U;
    if (Desc.DepthBias.Enable && Desc.DepthBias.Dynamic)
    {
        DynamicStates[DynamicCount++] = VK_DYNAMIC_STATE_DEPTH_BIAS;
    }
    if (Desc.Stencil.Enable && Desc.Stencil.DynamicReference)
    {
        DynamicStates[DynamicCount++] = VK_DYNAMIC_STATE_STENCIL_REFERENCE;
    }

    const VkPipelineDynamicStateCreateInfo DynamicState{.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                        .dynamicStateCount = DynamicCount,
                                                        .pDynamicStates    = std::data(DynamicStates)};

    const VkPipelineRenderingCreateInfo Rendering{.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                  .colorAttachmentCount    = static_cast<std::uint32_t>(std::size(Desc.ColorFormats)),
                                                  .pColorAttachmentFormats = std::data(Desc.ColorFormats),
                                                  .depthAttachmentFormat   = IsDepthFormat(Desc.DepthFormat) ? Desc.DepthFormat : VK_FORMAT_UNDEFINED,
                                                  .stencilAttachmentFormat = HasStencil(Desc.DepthFormat) ? Desc.DepthFormat : VK_FORMAT_UNDEFINED};

    const VkGraphicsPipelineCreateInfo CreateInfo{.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                  .pNext               = &Rendering,
                                                  .stageCount          = static_cast<std::uint32_t>(std::size(Builder.Stages)),
                                                  .pStages             = std::data(Builder.Stages),
                                                  .pVertexInputState   = IsMeshPipeline ? nullptr : &VertexInput,
                                                  .pInputAssemblyState = IsMeshPipeline ? nullptr : &InputAssembly,
                                                  .pViewportState      = &ViewportState,
                                                  .pRasterizationState = &Rasterization,
                                                  .pMultisampleState   = &Multisample,
                                                  .pDepthStencilState  = &DepthStencil,
                                                  .pColorBlendState    = &ColorBlend,
                                                  .pDynamicState       = &DynamicState,
                                                  .layout              = Ctx.Bindless().PipelineLayout()};

    Check(vkCreateGraphicsPipelines(Device, Ctx.PipelineCache(), 1U, &CreateInfo, nullptr, &m_Pipeline), "vkCreateGraphicsPipelines");
    Ctx.TrackResource(+1);
    Ctx.SetDebugName(m_Pipeline, VK_OBJECT_TYPE_PIPELINE, Desc.Name);
}

luvk::Pipeline::Pipeline(Context& Ctx, const ComputePipelineDesc& Desc)
    : m_Context(&Ctx),
      m_BindPoint(VK_PIPELINE_BIND_POINT_COMPUTE)
{
    if (!Desc.Compute.IsSet())
    {
        throw std::invalid_argument("[luvk] A compute pipeline needs a compute shader.");
    }

    const StageSpecialization  Constants = PackSpecialization(Desc.Compute);
    const VkSpecializationInfo Specialization = MakeSpecializationInfo(Constants);
    const ShaderModule         Module(Ctx.Device(), Desc.Compute.Code);
    const std::string          EntryPoint(Desc.Compute.EntryPoint);

    const VkComputePipelineCreateInfo CreateInfo{.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                 .stage  = {.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                            .stage               = VK_SHADER_STAGE_COMPUTE_BIT,
                                                            .module              = Module.Handle(),
                                                            .pName               = EntryPoint.c_str(),
                                                            .pSpecializationInfo = std::empty(Constants.Entries) ? nullptr : &Specialization},
                                                 .layout = Ctx.Bindless().PipelineLayout()};

    Check(vkCreateComputePipelines(Ctx.Device(), Ctx.PipelineCache(), 1U, &CreateInfo, nullptr, &m_Pipeline), "vkCreateComputePipelines");
    Ctx.TrackResource(+1);
    Ctx.SetDebugName(m_Pipeline, VK_OBJECT_TYPE_PIPELINE, Desc.Name);
}

luvk::Pipeline::~Pipeline()
{
    Reset();
}

luvk::Pipeline::Pipeline(Pipeline&& Other) noexcept
    : m_Context(std::exchange(Other.m_Context, nullptr)),
      m_Pipeline(std::exchange(Other.m_Pipeline, VK_NULL_HANDLE)),
      m_BindPoint(Other.m_BindPoint) {}

luvk::Pipeline& luvk::Pipeline::operator=(Pipeline&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        m_Context   = std::exchange(Other.m_Context, nullptr);
        m_Pipeline  = std::exchange(Other.m_Pipeline, VK_NULL_HANDLE);
        m_BindPoint = Other.m_BindPoint;
    }
    return *this;
}

void luvk::Pipeline::Reset() noexcept
{
    if (m_Pipeline != VK_NULL_HANDLE)
    {
        m_Context->Retire({.Kind = RetiredKind::Pipeline, .Handle = HandleToU64(m_Pipeline)});
        m_Context->TrackResource(-1);
        m_Pipeline = VK_NULL_HANDLE;
    }
}

VkPipelineLayout luvk::Pipeline::Layout() const noexcept
{
    return m_Context != nullptr ? m_Context->Bindless().PipelineLayout() : VK_NULL_HANDLE;
}
