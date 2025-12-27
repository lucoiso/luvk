/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Pipeline.hpp"
#include <array>
#include <iterator>
#include <stdexcept>
#include <vector>
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Device.hpp"
#include "luvk/Resources/PipelineCache.hpp"

using namespace luvk;

namespace
{
    struct ShaderStageHelper
    {
        VkDevice                                     Device;
        std::vector<VkShaderModule>                  Modules;
        std::vector<VkPipelineShaderStageCreateInfo> Stages;

        explicit ShaderStageHelper(const VkDevice Dev) : Device(Dev) {}

        ~ShaderStageHelper()
        {
            for (const auto Mod : Modules)
            {
                vkDestroyShaderModule(Device, Mod, nullptr);
            }
        }

        void AddStage(const VkShaderStageFlagBits Stage, std::span<const std::uint32_t> Code, const char* EntryPoint = "main")
        {
            if (std::empty(Code))
            {
                return;
            }

            const VkShaderModuleCreateInfo Info{.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                .codeSize = Code.size_bytes(),
                                                .pCode    = std::data(Code)};

            VkShaderModule Module;
            if (!LUVK_EXECUTE(vkCreateShaderModule(Device, &Info, nullptr, &Module)))
            {
                throw std::runtime_error("Failed to create shader module.");
            }

            Modules.push_back(Module);

            Stages.push_back({.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                              .stage  = Stage,
                              .module = Module,
                              .pName  = EntryPoint});
        }
    };

    struct GraphicsPipelineConfigurator
    {
        VkPipelineViewportStateCreateInfo ViewportState{.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                        .viewportCount = 1,
                                                        .scissorCount  = 1};

        VkPipelineRasterizationStateCreateInfo Rasterization{.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                                                             .polygonMode = VK_POLYGON_MODE_FILL,
                                                             .lineWidth   = 1.F};

        VkPipelineMultisampleStateCreateInfo Multisample{.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                         .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};

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

        VkPipelineDepthStencilStateCreateInfo DepthStencil{.sType          = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                                                           .depthCompareOp = VK_COMPARE_OP_LESS,
                                                           .maxDepthBounds = 1.F};

        std::array<VkDynamicState, 2> DynamicStates{VK_DYNAMIC_STATE_VIEWPORT,
                                                    VK_DYNAMIC_STATE_SCISSOR};

        VkPipelineDynamicStateCreateInfo Dynamic{.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                 .dynamicStateCount = 2,
                                                 .pDynamicStates    = std::data(DynamicStates)};

        template <typename TArgs>
        void Configure(const TArgs& Args)
        {
            Rasterization.cullMode  = Args.CullMode;
            Rasterization.frontFace = Args.FrontFace;

            DepthStencil.depthTestEnable  = Args.EnableDepthOp;
            DepthStencil.depthWriteEnable = Args.EnableDepthOp;
        }

        VkGraphicsPipelineCreateInfo GetCreateInfo(const VkPipelineLayout      Layout,
                                                   const VkRenderPass          RenderPass,
                                                   const uint32_t              Subpass,
                                                   const VkPipelineCreateFlags Flags) const
        {
            return VkGraphicsPipelineCreateInfo{.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                .flags               = Flags,
                                                .pViewportState      = &ViewportState,
                                                .pRasterizationState = &Rasterization,
                                                .pMultisampleState   = &Multisample,
                                                .pDepthStencilState  = &DepthStencil,
                                                .pColorBlendState    = &ColorBlend,
                                                .pDynamicState       = &Dynamic,
                                                .layout              = Layout,
                                                .renderPass          = RenderPass,
                                                .subpass             = Subpass};
        }
    };
}

Pipeline::Pipeline(const std::shared_ptr<Device>& DeviceModule)
    : m_DeviceModule(DeviceModule) {}

Pipeline::~Pipeline()
{
    Clear();
}

void Pipeline::SetupPipelineLayout(std::span<const VkDescriptorSetLayout> SetLayouts, std::span<const VkPushConstantRange> PushConstants)
{
    m_PushConstants.assign(std::begin(PushConstants), std::end(PushConstants));

    const VkPipelineLayoutCreateInfo LayoutInfo{.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                                                .setLayoutCount         = static_cast<std::uint32_t>(std::size(SetLayouts)),
                                                .pSetLayouts            = std::data(SetLayouts),
                                                .pushConstantRangeCount = static_cast<std::uint32_t>(std::size(m_PushConstants)),
                                                .pPushConstantRanges    = std::data(m_PushConstants)};

    if (!LUVK_EXECUTE(vkCreatePipelineLayout(m_DeviceModule->GetLogicalDevice(), &LayoutInfo, nullptr, &m_PipelineLayout)))
    {
        throw std::runtime_error("Failed to create pipeline layout.");
    }
}

void Pipeline::CreateGraphicsPipeline(const CreationArguments& Arguments)
{
    m_Type = Type::Graphics;
    SetupPipelineLayout(Arguments.SetLayouts, Arguments.PushConstants);

    ShaderStageHelper Shaders(m_DeviceModule->GetLogicalDevice());
    Shaders.AddStage(VK_SHADER_STAGE_VERTEX_BIT, Arguments.VertexShader);
    Shaders.AddStage(VK_SHADER_STAGE_FRAGMENT_BIT, Arguments.FragmentShader);

    const VkPipelineVertexInputStateCreateInfo VertexInput{.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                                                           .vertexBindingDescriptionCount   = static_cast<std::uint32_t>(std::size(Arguments.Bindings)),
                                                           .pVertexBindingDescriptions      = std::data(Arguments.Bindings),
                                                           .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(std::size(Arguments.Attributes)),
                                                           .pVertexAttributeDescriptions    = std::data(Arguments.Attributes)};

    const VkPipelineInputAssemblyStateCreateInfo InputAssembly{.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                                                               .topology               = Arguments.Topology,
                                                               .primitiveRestartEnable = VK_FALSE};

    GraphicsPipelineConfigurator Config;
    Config.Configure(Arguments);

    VkGraphicsPipelineCreateInfo PipelineInfo = Config.GetCreateInfo(m_PipelineLayout, Arguments.RenderPass, Arguments.Subpass, Arguments.Flags);
    PipelineInfo.stageCount                   = static_cast<std::uint32_t>(std::size(Shaders.Stages));
    PipelineInfo.pStages                      = std::data(Shaders.Stages);
    PipelineInfo.pVertexInputState            = &VertexInput;
    PipelineInfo.pInputAssemblyState          = &InputAssembly;

    VkPipelineCache CompositeCache = Arguments.Cache ? Arguments.Cache->GetCompositeCache() : VK_NULL_HANDLE;

    if (!LUVK_EXECUTE(vkCreateGraphicsPipelines(m_DeviceModule->GetLogicalDevice(), CompositeCache, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        throw std::runtime_error("Failed to create graphics pipeline.");
    }
}

void Pipeline::RecreateGraphicsPipeline(const CreationArguments& Arguments)
{
    Clear();
    CreateGraphicsPipeline(Arguments);
}

void Pipeline::CreateComputePipeline(const ComputeCreationArguments& Arguments)
{
    m_Type = Type::Compute;
    SetupPipelineLayout(Arguments.SetLayouts, Arguments.PushConstants);

    ShaderStageHelper Shaders(m_DeviceModule->GetLogicalDevice());
    Shaders.AddStage(VK_SHADER_STAGE_COMPUTE_BIT, Arguments.ComputeShader);

    const VkComputePipelineCreateInfo PipelineInfo{.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
                                                   .flags  = Arguments.Flags,
                                                   .stage  = Shaders.Stages[0],
                                                   .layout = m_PipelineLayout};

    const VkPipelineCache CompositeCache = Arguments.Cache ? Arguments.Cache->GetCompositeCache() : VK_NULL_HANDLE;

    if (!LUVK_EXECUTE(vkCreateComputePipelines(m_DeviceModule->GetLogicalDevice(), CompositeCache, 1 , &PipelineInfo, nullptr, &m_Pipeline)))
    {
        throw std::runtime_error("Failed to create compute pipeline.");
    }
}

void Pipeline::RecreateComputePipeline(const ComputeCreationArguments& Arguments)
{
    Clear();
    CreateComputePipeline(Arguments);
}

void Pipeline::CreateMeshPipeline(const MeshCreationArguments& Arguments)
{
    m_Type = Type::Mesh;
    SetupPipelineLayout(Arguments.SetLayouts, Arguments.PushConstants);

    ShaderStageHelper Shaders(m_DeviceModule->GetLogicalDevice());
    Shaders.AddStage(VK_SHADER_STAGE_TASK_BIT_EXT, Arguments.TaskShader);
    Shaders.AddStage(VK_SHADER_STAGE_MESH_BIT_EXT, Arguments.MeshShader);
    Shaders.AddStage(VK_SHADER_STAGE_FRAGMENT_BIT, Arguments.FragmentShader);

    GraphicsPipelineConfigurator Config;
    Config.Configure(Arguments);

    VkGraphicsPipelineCreateInfo PipelineInfo = Config.GetCreateInfo(m_PipelineLayout, Arguments.RenderPass, Arguments.Subpass, Arguments.Flags);
    PipelineInfo.stageCount                   = static_cast<std::uint32_t>(std::size(Shaders.Stages));
    PipelineInfo.pStages                      = std::data(Shaders.Stages);

    PipelineInfo.pVertexInputState   = nullptr;
    PipelineInfo.pInputAssemblyState = nullptr;

    VkPipelineCache CompositeCache = Arguments.Cache ? Arguments.Cache->GetCompositeCache() : VK_NULL_HANDLE;

    if (!LUVK_EXECUTE(vkCreateGraphicsPipelines(m_DeviceModule->GetLogicalDevice(), CompositeCache, 1, &PipelineInfo, nullptr, &m_Pipeline)))
    {
        throw std::runtime_error("Failed to create mesh pipeline.");
    }
}

void Pipeline::RecreateMeshPipeline(const MeshCreationArguments& Arguments)
{
    Clear();
    CreateMeshPipeline(Arguments);
}

void Pipeline::Clear()
{
    if (!m_DeviceModule) return;

    const VkDevice LogicalDevice = m_DeviceModule->GetLogicalDevice();

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
