// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Pipeline.hpp"
#include "luvk/Core/Device.hpp"
#include "luvk/Core/PipelineCache.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include <iterator>

/** Helper to create shader modules from SPIR-V code */
static VkShaderModule CreateShader(const VkDevice Device, std::span<std::uint32_t const> code)
{
    VkShaderModuleCreateInfo const Info{.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                        .codeSize = std::size(code) * sizeof(std::uint32_t),
                                        .pCode = std::data(code)};
    VkShaderModule Module{VK_NULL_HANDLE};
    if (!LUVK_EXECUTE(vkCreateShaderModule(Device, &Info, nullptr, &Module)))
    {
        throw std::runtime_error("Failed to create shader module.");
    }
    return Module;
}

luvk::Pipeline::~Pipeline()
{
    ClearResources();
}

void luvk::Pipeline::CreateGraphicsPipeline(std::shared_ptr<Device> const& DeviceModule, CreationArguments const& Arguments)
{
    m_DeviceModule = DeviceModule;
    auto const* Device = DeviceModule.get();
    VkDevice const& LogicalDevice = Device->GetLogicalDevice();

    VkShaderModule VertModule = CreateShader(LogicalDevice, Arguments.VertexShader);
    VkShaderModule FragModule = CreateShader(LogicalDevice, Arguments.FragmentShader);

    VkPipelineShaderStageCreateInfo const VertStage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                    .pNext = nullptr,
                                                    .flags = 0,
                                                    .stage = VK_SHADER_STAGE_VERTEX_BIT,
                                                    .module = VertModule,
                                                    .pName = "main"};

    VkPipelineShaderStageCreateInfo const FragStage{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                                                    .pNext = nullptr,
                                                    .flags = 0,
                                                    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                                                    .module = FragModule,
                                                    .pName = "main"};

    VkPipelineVertexInputStateCreateInfo const VertexInput{.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                           .vertexBindingDescriptionCount = static_cast<std::uint32_t>(std::size(Arguments.Bindings)),
                                                           .pVertexBindingDescriptions = std::data(Arguments.Bindings),
                                                           .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(std::size(Arguments.Attributes)),
                                                           .pVertexAttributeDescriptions = std::data(Arguments.Attributes)};

    VkPipelineInputAssemblyStateCreateInfo const InputAssembly{.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                               .topology = Arguments.Topology,
                                                               .primitiveRestartEnable = VK_FALSE};

    constexpr VkPipelineViewportStateCreateInfo ViewportState{.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                              .pNext = nullptr,
                                                              .flags = 0,
                                                              .viewportCount = 1,
                                                              .pViewports = nullptr,
                                                              .scissorCount = 1,
                                                              .pScissors = nullptr};

    VkPipelineRasterizationStateCreateInfo const Rasterization{.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
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
                                                                       .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                                                                       .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                                                                       .colorBlendOp = VK_BLEND_OP_ADD,
                                                                       .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                                                                       .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                                                                       .alphaBlendOp = VK_BLEND_OP_ADD,
                                                                       .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                                       VK_COLOR_COMPONENT_G_BIT |
                                                                       VK_COLOR_COMPONENT_B_BIT |
                                                                       VK_COLOR_COMPONENT_A_BIT};

    const VkPipelineColorBlendStateCreateInfo ColorBlend{.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                         .attachmentCount = 1,
                                                         .pAttachments = &ColorBlendAttachment};

    constexpr std::array DynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    const VkPipelineDynamicStateCreateInfo Dynamic{.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .flags = 0,
                                                   .dynamicStateCount = static_cast<std::uint32_t>(std::size(DynamicStates)),
                                                   .pDynamicStates = std::data(DynamicStates)};

    VkPipelineLayoutCreateInfo const LayoutInfo{.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount = static_cast<std::uint32_t>(std::size(Arguments.SetLayouts)),
                                                .pSetLayouts = std::data(Arguments.SetLayouts),
                                                .pushConstantRangeCount = static_cast<std::uint32_t>(std::size(Arguments.PushConstants)),
                                                .pPushConstantRanges = std::data(Arguments.PushConstants)};

    if (!LUVK_EXECUTE(vkCreatePipelineLayout(LogicalDevice, &LayoutInfo, nullptr, &m_PipelineLayout)))
    {
        vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
        vkDestroyShaderModule(LogicalDevice, VertModule, nullptr);
        throw std::runtime_error("Failed to create pipeline layout.");
    }

    const std::array Stages{VertStage, FragStage};
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
                                              .pDepthStencilState = nullptr,
                                              .pColorBlendState = &ColorBlend,
                                              .pDynamicState = &Dynamic,
                                              .layout = m_PipelineLayout,
                                              .renderPass = Arguments.RenderPass,
                                              .subpass = Arguments.Subpass};

    VkPipelineCache compCache = Arguments.Cache
                                    ? Arguments.Cache->GetCompositeCache()
                                    : VK_NULL_HANDLE;
    if (!LUVK_EXECUTE(vkCreateGraphicsPipelines(LogicalDevice, compCache, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
        vkDestroyShaderModule(LogicalDevice, VertModule, nullptr);
        throw std::runtime_error("Failed to create graphics pipeline.");
    }

    vkDestroyShaderModule(LogicalDevice, FragModule, nullptr);
    vkDestroyShaderModule(LogicalDevice, VertModule, nullptr);
}

void luvk::Pipeline::RecreateGraphicsPipeline(std::shared_ptr<Device> const& DeviceModule, CreationArguments const& Arguments)
{
    if (m_DeviceModule)
    {
        const auto* Dev = m_DeviceModule.get();
        const VkDevice LogicalDevice = Dev->GetLogicalDevice();

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
    }

    CreateGraphicsPipeline(DeviceModule, Arguments);
}

void luvk::Pipeline::ClearResources()
{
    if (!m_DeviceModule)
    {
        return;
    }
    auto const* DeviceModule = m_DeviceModule.get();
    VkDevice const& LogicalDevice = DeviceModule->GetLogicalDevice();

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
}
