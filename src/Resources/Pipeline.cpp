/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Pipeline.hpp"
#include <array>
#include <stdexcept>
#include <vector>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"

using namespace luvk;

Pipeline::Pipeline(Device* DeviceModule)
    : m_DeviceModule(DeviceModule) {}

Pipeline::~Pipeline()
{
    const VkDevice Device = m_DeviceModule->GetLogicalDevice();
    if (m_Pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(Device, m_Pipeline, nullptr);
    }
    if (m_PipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(Device, m_PipelineLayout, nullptr);
    }
}

namespace
{
    VkShaderModule CreateShaderModule(const VkDevice Device, std::span<const std::uint32_t> Code)
    {
        if (std::empty(Code)) return VK_NULL_HANDLE;

        const VkShaderModuleCreateInfo Info{.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                            .codeSize = std::size(Code) * sizeof(std::uint32_t),
                                            .pCode    = std::data(Code)};

        VkShaderModule Module;
        if (!LUVK_EXECUTE(vkCreateShaderModule(Device, &Info, nullptr, &Module)))
        {
            throw std::runtime_error("Failed to create shader module");
        }
        return Module;
    }
}

void Pipeline::CreateGraphicsPipeline(const GraphicsCreationArguments& Arguments)
{
    m_Type                = Type::Graphics;
    const VkDevice Device = m_DeviceModule->GetLogicalDevice();

    m_PushConstants.assign(std::begin(Arguments.PushConstants), std::end(Arguments.PushConstants));

    const VkPipelineLayoutCreateInfo LayoutInfo{.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount         = static_cast<std::uint32_t>(std::size(Arguments.SetLayouts)),
                                                .pSetLayouts            = std::data(Arguments.SetLayouts),
                                                .pushConstantRangeCount = static_cast<std::uint32_t>(std::size(m_PushConstants)),
                                                .pPushConstantRanges    = std::data(m_PushConstants)};

    if (!LUVK_EXECUTE(vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &m_PipelineLayout)))
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    const VkPipelineRenderingCreateInfo RenderingInfo{.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                      .colorAttachmentCount    = static_cast<std::uint32_t>(std::size(Arguments.ColorFormats)),
                                                      .pColorAttachmentFormats = std::data(Arguments.ColorFormats),
                                                      .depthAttachmentFormat   = Arguments.DepthFormat,
                                                      .stencilAttachmentFormat = Arguments.DepthFormat};

    VkShaderModule VertModule = CreateShaderModule(Device, Arguments.VertexShader);
    VkShaderModule FragModule = CreateShaderModule(Device, Arguments.FragmentShader);

    std::vector<VkPipelineShaderStageCreateInfo> Stages;
    Stages.reserve(2);

    if (VertModule)
    {
        Stages.push_back({.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage  = VK_SHADER_STAGE_VERTEX_BIT,
                          .module = VertModule,
                          .pName  = "main"});
    }

    if (FragModule)
    {
        Stages.push_back({.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                          .module = FragModule,
                          .pName  = "main"});
    }

    VkPipelineVertexInputStateCreateInfo VertexInput{.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                     .vertexBindingDescriptionCount   = static_cast<std::uint32_t>(std::size(Arguments.VertexBindings)),
                                                     .pVertexBindingDescriptions      = std::data(Arguments.VertexBindings),
                                                     .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(std::size(Arguments.VertexAttributes)),
                                                     .pVertexAttributeDescriptions    = std::data(Arguments.VertexAttributes)};

    VkPipelineInputAssemblyStateCreateInfo InputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology = Arguments.Topology};

    VkPipelineViewportStateCreateInfo ViewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};

    VkPipelineRasterizationStateCreateInfo Rasterization{.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                         .polygonMode = VK_POLYGON_MODE_FILL,
                                                         .cullMode    = Arguments.CullMode,
                                                         .frontFace   = Arguments.FrontFace,
                                                         .lineWidth   = 1.0f};

    VkPipelineMultisampleStateCreateInfo Multisample{.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                     .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

    VkPipelineDepthStencilStateCreateInfo DepthStencil{.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                       .depthTestEnable  = Arguments.EnableDepthTest,
                                                       .depthWriteEnable = Arguments.EnableDepthWrite,
                                                       .depthCompareOp   = VK_COMPARE_OP_LESS};

    VkPipelineColorBlendAttachmentState BlendAttachment{.blendEnable         = VK_TRUE,
                                                        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                                                        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                        .colorBlendOp        = VK_BLEND_OP_ADD,
                                                        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                        .alphaBlendOp        = VK_BLEND_OP_ADD,
                                                        .colorWriteMask      = 0xF};

    VkPipelineColorBlendStateCreateInfo ColorBlend{.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                   .attachmentCount = 1,
                                                   .pAttachments    = &BlendAttachment};

    std::array                       DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo DynamicState{.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                  .dynamicStateCount = static_cast<std::uint32_t>(std::size(DynamicStates)),
                                                  .pDynamicStates    = std::data(DynamicStates)};

    VkGraphicsPipelineCreateInfo PipelineInfo{.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                              .pNext               = &RenderingInfo,
                                              .stageCount          = static_cast<std::uint32_t>(std::size(Stages)),
                                              .pStages             = std::data(Stages),
                                              .pVertexInputState   = &VertexInput,
                                              .pInputAssemblyState = &InputAssembly,
                                              .pViewportState      = &ViewportState,
                                              .pRasterizationState = &Rasterization,
                                              .pMultisampleState   = &Multisample,
                                              .pDepthStencilState  = &DepthStencil,
                                              .pColorBlendState    = &ColorBlend,
                                              .pDynamicState       = &DynamicState,
                                              .layout              = m_PipelineLayout};

    if (!LUVK_EXECUTE(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        if (VertModule)
        {
            vkDestroyShaderModule(Device, VertModule, nullptr);
        }

        if (FragModule)
        {
            vkDestroyShaderModule(Device, FragModule, nullptr);
        }

        throw std::runtime_error("Failed to create graphics pipeline");
    }

    for (const auto& Range : m_PushConstants)
    {
        m_PushConstantStages |= Range.stageFlags;
    }

    if (VertModule)
    {
        vkDestroyShaderModule(Device, VertModule, nullptr);
    }

    if (FragModule)
    {
        vkDestroyShaderModule(Device, FragModule, nullptr);
    }
}

void Pipeline::CreateMeshPipeline(const MeshCreationArguments& Arguments)
{
    m_Type                = Type::Mesh;
    const VkDevice Device = m_DeviceModule->GetLogicalDevice();

    m_PushConstants.assign(std::begin(Arguments.PushConstants), std::end(Arguments.PushConstants));

    const VkPipelineLayoutCreateInfo LayoutInfo{.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount         = static_cast<std::uint32_t>(std::size(Arguments.SetLayouts)),
                                                .pSetLayouts            = std::data(Arguments.SetLayouts),
                                                .pushConstantRangeCount = static_cast<std::uint32_t>(std::size(m_PushConstants)),
                                                .pPushConstantRanges    = std::data(m_PushConstants)};

    if (!LUVK_EXECUTE(vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &m_PipelineLayout)))
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    const VkPipelineRenderingCreateInfo RenderingInfo{.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                      .colorAttachmentCount    = static_cast<std::uint32_t>(std::size(Arguments.ColorFormats)),
                                                      .pColorAttachmentFormats = std::data(Arguments.ColorFormats),
                                                      .depthAttachmentFormat   = Arguments.DepthFormat,
                                                      .stencilAttachmentFormat = Arguments.DepthFormat};

    VkShaderModule TaskModule = CreateShaderModule(Device, Arguments.TaskShader);
    VkShaderModule MeshModule = CreateShaderModule(Device, Arguments.MeshShader);
    VkShaderModule FragModule = CreateShaderModule(Device, Arguments.FragmentShader);

    std::vector<VkPipelineShaderStageCreateInfo> Stages;
    if (TaskModule)
        Stages.push_back({.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage  = VK_SHADER_STAGE_TASK_BIT_EXT,
                          .module = TaskModule,
                          .pName  = "main"});
    if (MeshModule)
        Stages.push_back({.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage  = VK_SHADER_STAGE_MESH_BIT_EXT,
                          .module = MeshModule,
                          .pName  = "main"});
    if (FragModule)
        Stages.push_back({.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .stage  = VK_SHADER_STAGE_FRAGMENT_BIT,
                          .module = FragModule,
                          .pName  = "main"});

    VkPipelineViewportStateCreateInfo      ViewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount = 1, .scissorCount = 1};
    VkPipelineRasterizationStateCreateInfo Rasterization{.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                         .polygonMode = VK_POLYGON_MODE_FILL,
                                                         .cullMode    = Arguments.CullMode,
                                                         .frontFace   = Arguments.FrontFace,
                                                         .lineWidth   = 1.0f};
    VkPipelineMultisampleStateCreateInfo Multisample{.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                     .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
    VkPipelineDepthStencilStateCreateInfo DepthStencil{.sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                       .depthTestEnable  = Arguments.EnableDepthTest,
                                                       .depthWriteEnable = Arguments.EnableDepthWrite,
                                                       .depthCompareOp   = VK_COMPARE_OP_LESS};
    VkPipelineColorBlendAttachmentState BlendAttachment{.blendEnable         = VK_TRUE,
                                                        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                                                        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                        .colorBlendOp        = VK_BLEND_OP_ADD,
                                                        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                        .alphaBlendOp        = VK_BLEND_OP_ADD,
                                                        .colorWriteMask      = 0xF};
    VkPipelineColorBlendStateCreateInfo ColorBlend{.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                   .attachmentCount = 1,
                                                   .pAttachments    = &BlendAttachment};
    std::array                       DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo DynamicState{.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                  .dynamicStateCount = static_cast<std::uint32_t>(std::size(DynamicStates)),
                                                  .pDynamicStates    = std::data(DynamicStates)};

    VkGraphicsPipelineCreateInfo PipelineInfo{.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                              .pNext               = &RenderingInfo,
                                              .stageCount          = static_cast<std::uint32_t>(std::size(Stages)),
                                              .pStages             = std::data(Stages),
                                              .pViewportState      = &ViewportState,
                                              .pRasterizationState = &Rasterization,
                                              .pMultisampleState   = &Multisample,
                                              .pDepthStencilState  = &DepthStencil,
                                              .pColorBlendState    = &ColorBlend,
                                              .pDynamicState       = &DynamicState,
                                              .layout              = m_PipelineLayout};

    if (!LUVK_EXECUTE(vkCreateGraphicsPipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        if (TaskModule)
        {
            vkDestroyShaderModule(Device, TaskModule, nullptr);
        }

        if (MeshModule)
        {
            vkDestroyShaderModule(Device, MeshModule, nullptr);
        }

        if (FragModule)
        {
            vkDestroyShaderModule(Device, FragModule, nullptr);
        }

        throw std::runtime_error("Failed to create mesh pipeline");
    }

    for (const auto& Range : m_PushConstants)
    {
        m_PushConstantStages |= Range.stageFlags;
    }

    if (TaskModule)
    {
        vkDestroyShaderModule(Device, TaskModule, nullptr);
    }

    if (MeshModule)
    {
        vkDestroyShaderModule(Device, MeshModule, nullptr);
    }

    if (FragModule)
    {
        vkDestroyShaderModule(Device, FragModule, nullptr);
    }
}

void Pipeline::CreateComputePipeline(const ComputeCreationArguments& Arguments)
{
    m_Type                = Type::Compute;
    const VkDevice Device = m_DeviceModule->GetLogicalDevice();

    m_PushConstants.assign(std::begin(Arguments.PushConstants), std::end(Arguments.PushConstants));

    const VkPipelineLayoutCreateInfo LayoutInfo{.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount         = static_cast<std::uint32_t>(std::size(Arguments.SetLayouts)),
                                                .pSetLayouts            = std::data(Arguments.SetLayouts),
                                                .pushConstantRangeCount = static_cast<std::uint32_t>(std::size(m_PushConstants)),
                                                .pPushConstantRanges    = std::data(m_PushConstants)};

    if (!LUVK_EXECUTE(vkCreatePipelineLayout(Device, &LayoutInfo, nullptr, &m_PipelineLayout)))
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    const VkShaderModule CompModule = CreateShaderModule(Device, Arguments.ComputeShader);

    const VkComputePipelineCreateInfo PipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                   .stage = {.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                             .stage  = VK_SHADER_STAGE_COMPUTE_BIT,
                                                             .module = CompModule,
                                                             .pName  = "main"},
                                                   .layout = m_PipelineLayout};

    if (!LUVK_EXECUTE(vkCreateComputePipelines(Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        vkDestroyShaderModule(Device, CompModule, nullptr);
        throw std::runtime_error("Failed to create compute pipeline");
    }

    for (const auto& Range : m_PushConstants)
    {
        m_PushConstantStages |= Range.stageFlags;
    }

    vkDestroyShaderModule(Device, CompModule, nullptr);
}
