// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Pipeline.hpp"
#include <iterator>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Resources/PipelineCache.hpp"
#include "luvk/Types/Array.hpp"
#include "luvk/Types/Vector.hpp"

static VkShaderModule CreateShader(const VkDevice& Device, std::span<const std::uint32_t> Code)
{
    const VkShaderModuleCreateInfo Info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                        .codeSize = std::size(Code) * sizeof(std::uint32_t),
                                        .pCode = std::data(Code)};

    VkShaderModule Module{VK_NULL_HANDLE};
    if (!LUVK_EXECUTE(vkCreateShaderModule(Device, &Info, nullptr, &Module)))
    {
        throw std::runtime_error("Failed to create shader module.");
    }
    return Module;
}

luvk::Pipeline::Pipeline(const std::shared_ptr<Device>& DeviceModule)
    : m_DeviceModule(DeviceModule) {}

luvk::Pipeline::~Pipeline()
{
    Clear();
}

void luvk::Pipeline::CreateGraphicsPipeline(const CreationArguments& Arguments)
{
    m_Type                        = Type::Graphics;
    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

    VkShaderModule VertModule = CreateShader(LogicalDevice, Arguments.VertexShader);
    VkShaderModule FragModule = CreateShader(LogicalDevice, Arguments.FragmentShader);

    const VkPipelineShaderStageCreateInfo VertStage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                    .pNext = nullptr,
                                                    .flags = 0,
                                                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                    .module = VertModule,
                                                    .pName = "main"};

    const VkPipelineShaderStageCreateInfo FragStage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                    .pNext = nullptr,
                                                    .flags = 0,
                                                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                    .module = FragModule,
                                                    .pName = "main"};

    const VkPipelineVertexInputStateCreateInfo VertexInput{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                           .vertexBindingDescriptionCount = static_cast<std::uint32_t>(std::size(Arguments.Bindings)),
                                                           .pVertexBindingDescriptions = std::data(Arguments.Bindings),
                                                           .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(std::size(Arguments.Attributes)),
                                                           .pVertexAttributeDescriptions = std::data(Arguments.Attributes)};

    const VkPipelineInputAssemblyStateCreateInfo InputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                               .topology = Arguments.Topology,
                                                               .primitiveRestartEnable = VK_FALSE};

    constexpr VkPipelineViewportStateCreateInfo ViewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                              .pNext = nullptr,
                                                              .flags = 0,
                                                              .viewportCount = 1,
                                                              .pViewports = nullptr,
                                                              .scissorCount = 1,
                                                              .pScissors = nullptr};

    const VkPipelineRasterizationStateCreateInfo Rasterization{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                               .depthClampEnable = VK_FALSE,
                                                               .rasterizerDiscardEnable = VK_FALSE,
                                                               .polygonMode = VK_POLYGON_MODE_FILL,
                                                               .cullMode = Arguments.CullMode,
                                                               .frontFace = Arguments.FrontFace,
                                                               .depthBiasEnable = VK_FALSE,
                                                               .depthBiasConstantFactor = 0.F,
                                                               .depthBiasClamp = 0.F,
                                                               .depthBiasSlopeFactor = 0.F,
                                                               .lineWidth = 1.F};

    constexpr VkPipelineMultisampleStateCreateInfo Multisample{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                               .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

    constexpr VkPipelineColorBlendAttachmentState ColorBlendAttachment{.blendEnable = VK_TRUE,
                                                                       .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                                                                       .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                                       .colorBlendOp = VK_BLEND_OP_ADD,
                                                                       .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                                       .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                                       .alphaBlendOp = VK_BLEND_OP_ADD,
                                                                       .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                       VK_COLOR_COMPONENT_G_BIT |
                                                                       VK_COLOR_COMPONENT_B_BIT |
                                                                       VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo ColorBlend{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                         .attachmentCount = 1,
                                                         .pAttachments = &ColorBlendAttachment};

    constexpr luvk::Array DynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    const VkPipelineDynamicStateCreateInfo Dynamic{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = 0,
                                                   .dynamicStateCount = static_cast<std::uint32_t>(std::size(DynamicStates)),
                                                   .pDynamicStates = std::data(DynamicStates)};

    m_PushConstants.assign(std::begin(Arguments.PushConstants), std::end(Arguments.PushConstants));

    const VkPipelineLayoutCreateInfo LayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount = static_cast<std::uint32_t>(std::size(Arguments.SetLayouts)),
                                                .pSetLayouts = std::data(Arguments.SetLayouts),
                                                .pushConstantRangeCount = static_cast<std::uint32_t>(std::size(m_PushConstants)),
                                                .pPushConstantRanges = std::data(m_PushConstants)};

    if (!LUVK_EXECUTE(vkCreatePipelineLayout(LogicalDevice, &LayoutInfo, nullptr, &m_PipelineLayout)))
    {
        vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
        vkDestroyShaderModule(LogicalDevice, VertModule, nullptr);
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    const VkPipelineDepthStencilStateCreateInfo DepthStencilState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                                  .depthTestEnable = Arguments.EnableDepthOp,
                                                                  .depthWriteEnable = Arguments.EnableDepthOp,
                                                                  .depthCompareOp = VK_COMPARE_OP_LESS,
                                                                  .depthBoundsTestEnable = VK_FALSE,
                                                                  .stencilTestEnable = VK_FALSE,
                                                                  .minDepthBounds = 0.F,
                                                                  .maxDepthBounds = 1.F};

    const luvk::Array            Stages{VertStage, FragStage};
    VkGraphicsPipelineCreateInfo PipelineInfo{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = 0,
                                              .stageCount = static_cast<std::uint32_t>(std::size(Stages)),
                                              .pStages = std::data(Stages),
                                              .pVertexInputState = &VertexInput,
                                              .pInputAssemblyState = &InputAssembly,
                                              .pTessellationState = nullptr,
                                              .pViewportState = &ViewportState,
                                              .pRasterizationState = &Rasterization,
                                              .pMultisampleState = &Multisample,
                                              .pDepthStencilState = &DepthStencilState,
                                              .pColorBlendState = &ColorBlend,
                                              .pDynamicState = &Dynamic,
                                              .layout = m_PipelineLayout,
                                              .renderPass = Arguments.RenderPass,
                                              .subpass = Arguments.Subpass};

    if (VkPipelineCache CompositeCache = Arguments.Cache
                                             ? Arguments.Cache->GetCompositeCache()
                                             : VK_NULL_HANDLE;
        !LUVK_EXECUTE(vkCreateGraphicsPipelines(LogicalDevice, CompositeCache, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
        vkDestroyShaderModule(LogicalDevice, VertModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline.");
    }

    vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
    vkDestroyShaderModule(LogicalDevice, VertModule, nullptr);
}

void luvk::Pipeline::RecreateGraphicsPipeline(const CreationArguments& Arguments)
{
    Clear();
    CreateGraphicsPipeline(Arguments);
}

void luvk::Pipeline::CreateComputePipeline(const ComputeCreationArguments& Arguments)
{
    m_Type = Type::Compute;

    const VkDevice&      LogicalDevice = m_DeviceModule->GetLogicalDevice();
    const VkShaderModule CompModule    = CreateShader(LogicalDevice, Arguments.ComputeShader);

    const VkPipelineShaderStageCreateInfo CompStage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                    .pNext = nullptr,
                                                    .flags = 0,
                                                    .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                                                    .module = CompModule,
                                                    .pName = "main"};

    m_PushConstants.assign(std::begin(Arguments.PushConstants), std::end(Arguments.PushConstants));

    const VkPipelineLayoutCreateInfo LayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount = static_cast<std::uint32_t>(std::size(Arguments.SetLayouts)),
                                                .pSetLayouts = std::data(Arguments.SetLayouts),
                                                .pushConstantRangeCount = static_cast<std::uint32_t>(std::size(m_PushConstants)),
                                                .pPushConstantRanges = std::data(m_PushConstants)};

    if (!LUVK_EXECUTE(vkCreatePipelineLayout(LogicalDevice, &LayoutInfo, nullptr, &m_PipelineLayout)))
    {
        vkDestroyShaderModule(LogicalDevice, CompModule, nullptr);
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    const VkComputePipelineCreateInfo PipelineInfo{.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = Arguments.Flags,
                                                   .stage = CompStage,
                                                   .layout = m_PipelineLayout};

    if (const VkPipelineCache CompositeCache = Arguments.Cache
                                                   ? Arguments.Cache->GetCompositeCache()
                                                   : VK_NULL_HANDLE;
        !LUVK_EXECUTE(vkCreateComputePipelines(LogicalDevice, CompositeCache, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        vkDestroyShaderModule(LogicalDevice, CompModule, nullptr);
        throw std::runtime_error("Failed to create compute pipeline.");
    }

    vkDestroyShaderModule(LogicalDevice, CompModule, nullptr);
}

void luvk::Pipeline::RecreateComputePipeline(const ComputeCreationArguments& Arguments)
{
    Clear();
    CreateComputePipeline(Arguments);
}

void luvk::Pipeline::CreateMeshPipeline(const MeshCreationArguments& Arguments)
{
    m_Type                        = Type::Mesh;
    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

    VkShaderModule MeshModule = CreateShader(LogicalDevice, Arguments.MeshShader);
    VkShaderModule TaskModule = VK_NULL_HANDLE;
    VkShaderModule FragModule = VK_NULL_HANDLE;

    if (!std::empty(Arguments.TaskShader))
    {
        TaskModule = CreateShader(LogicalDevice, Arguments.TaskShader);
    }

    if (!std::empty(Arguments.FragmentShader))
    {
        FragModule = CreateShader(LogicalDevice, Arguments.FragmentShader);
    }

    Vector<VkPipelineShaderStageCreateInfo> Stages{};
    if (TaskModule != VK_NULL_HANDLE)
    {
        Stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .pNext = nullptr,
                          .flags = 0,
                          .stage = VK_SHADER_STAGE_TASK_BIT_EXT,
                          .module = TaskModule,
                          .pName = "main"});
    }

    Stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .pNext = nullptr,
                      .flags = 0,
                      .stage = VK_SHADER_STAGE_MESH_BIT_EXT,
                      .module = MeshModule,
                      .pName = "main"});

    if (FragModule != VK_NULL_HANDLE)
    {
        Stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                          .pNext = nullptr,
                          .flags = 0,
                          .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                          .module = FragModule,
                          .pName = "main"});
    }

    const VkPipelineRasterizationStateCreateInfo Rasterization{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                               .depthClampEnable = VK_FALSE,
                                                               .rasterizerDiscardEnable = VK_FALSE,
                                                               .polygonMode = VK_POLYGON_MODE_FILL,
                                                               .cullMode = Arguments.CullMode,
                                                               .frontFace = Arguments.FrontFace,
                                                               .depthBiasEnable = VK_FALSE,
                                                               .depthBiasConstantFactor = 0.F,
                                                               .depthBiasClamp = 0.F,
                                                               .depthBiasSlopeFactor = 0.F,
                                                               .lineWidth = 1.F};

    constexpr VkPipelineViewportStateCreateInfo ViewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                              .pNext = nullptr,
                                                              .flags = 0,
                                                              .viewportCount = 1,
                                                              .pViewports = nullptr,
                                                              .scissorCount = 1,
                                                              .pScissors = nullptr};

    constexpr VkPipelineMultisampleStateCreateInfo Multisample{.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                               .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

    constexpr VkPipelineColorBlendAttachmentState ColorBlendAttachment{.blendEnable = VK_TRUE,
                                                                       .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                                                                       .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                                       .colorBlendOp = VK_BLEND_OP_ADD,
                                                                       .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                                       .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                                                       .alphaBlendOp = VK_BLEND_OP_ADD,
                                                                       .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                       VK_COLOR_COMPONENT_G_BIT |
                                                                       VK_COLOR_COMPONENT_B_BIT |
                                                                       VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo ColorBlend{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                         .attachmentCount = 1,
                                                         .pAttachments = &ColorBlendAttachment};

    constexpr luvk::Array DynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    const VkPipelineDynamicStateCreateInfo Dynamic{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = 0,
                                                   .dynamicStateCount = static_cast<std::uint32_t>(std::size(DynamicStates)),
                                                   .pDynamicStates = std::data(DynamicStates)};

    m_PushConstants.assign(std::begin(Arguments.PushConstants), std::end(Arguments.PushConstants));

    const VkPipelineLayoutCreateInfo LayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount = static_cast<std::uint32_t>(std::size(Arguments.SetLayouts)),
                                                .pSetLayouts = std::data(Arguments.SetLayouts),
                                                .pushConstantRangeCount = static_cast<std::uint32_t>(std::size(m_PushConstants)),
                                                .pPushConstantRanges = std::data(m_PushConstants)};

    if (!LUVK_EXECUTE(vkCreatePipelineLayout(LogicalDevice, &LayoutInfo, nullptr, &m_PipelineLayout)))
    {
        if (FragModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
        }
        if (TaskModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(LogicalDevice, TaskModule, nullptr);
        }
        vkDestroyShaderModule(LogicalDevice, MeshModule, nullptr);
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    const VkPipelineDepthStencilStateCreateInfo DepthStencilState{.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                                  .depthTestEnable = Arguments.EnableDepthOp,
                                                                  .depthWriteEnable = Arguments.EnableDepthOp,
                                                                  .depthCompareOp = VK_COMPARE_OP_LESS,
                                                                  .depthBoundsTestEnable = VK_FALSE,
                                                                  .stencilTestEnable = VK_FALSE,
                                                                  .minDepthBounds = 0.F,
                                                                  .maxDepthBounds = 1.F};

    VkGraphicsPipelineCreateInfo PipelineInfo{.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                              .pNext = nullptr,
                                              .flags = Arguments.Flags,
                                              .stageCount = static_cast<std::uint32_t>(std::size(Stages)),
                                              .pStages = std::data(Stages),
                                              .pVertexInputState = nullptr,
                                              .pInputAssemblyState = nullptr,
                                              .pTessellationState = nullptr,
                                              .pViewportState = &ViewportState,
                                              .pRasterizationState = &Rasterization,
                                              .pMultisampleState = &Multisample,
                                              .pDepthStencilState = &DepthStencilState,
                                              .pColorBlendState = &ColorBlend,
                                              .pDynamicState = &Dynamic,
                                              .layout = m_PipelineLayout,
                                              .renderPass = Arguments.RenderPass,
                                              .subpass = Arguments.Subpass};

    if (VkPipelineCache CompositeCache = Arguments.Cache
                                             ? Arguments.Cache->GetCompositeCache()
                                             : VK_NULL_HANDLE;
        !LUVK_EXECUTE(vkCreateGraphicsPipelines(LogicalDevice, CompositeCache, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        if (FragModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
        }
        if (TaskModule != VK_NULL_HANDLE)
        {
            vkDestroyShaderModule(LogicalDevice, TaskModule, nullptr);
        }

        vkDestroyShaderModule(LogicalDevice, MeshModule, nullptr);

        throw std::runtime_error("Failed to create mesh pipeline.");
    }

    if (FragModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
    }
    if (TaskModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(LogicalDevice, TaskModule, nullptr);
    }

    vkDestroyShaderModule(LogicalDevice, MeshModule, nullptr);
}

void luvk::Pipeline::RecreateMeshPipeline(const MeshCreationArguments& Arguments)
{
    Clear();
    CreateMeshPipeline(Arguments);
}

void luvk::Pipeline::Clear()
{
    if (!m_DeviceModule)
    {
        return;
    }

    const VkDevice& LogicalDevice = m_DeviceModule->GetLogicalDevice();

    if (m_Pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(LogicalDevice, m_Pipeline, nullptr);
        m_Pipeline = VK_NULL_HANDLE;
    }

    if (m_PipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(LogicalDevice, m_PipelineLayout, nullptr);
        m_PipelineLayout = VK_NULL_HANDLE;
    }
    m_PushConstants.clear();
}
