// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

#include <memory>
#include <span>
#include <volk/volk.h>

namespace luvk
{
    class Device;

    /** Vulkan graphics pipeline resource */
    class LUVKMODULE_API Pipeline
    {
        /** Layout describing descriptor sets and push constants */
        VkPipelineLayout m_PipelineLayout{VK_NULL_HANDLE};

        /** Handle to the graphics pipeline */
        VkPipeline m_Pipeline{VK_NULL_HANDLE};

        /** Device used to create the pipeline */
        std::shared_ptr<Device> m_DeviceModule{};

    public: /** Default constructor */
        constexpr Pipeline() = default;

        /** Destructor releases the pipeline */
        ~Pipeline();

        /** Parameters used to create a pipeline */
        struct CreationArguments
        {
            /** Render area size */
            VkExtent2D Extent{0U, 0U};

            /** Formats of color attachments */
            std::span<VkFormat const> ColorFormats{};

            /** Format of the depth attachment */
            VkFormat DepthFormat{VK_FORMAT_UNDEFINED};

            /** Render pass used for pipeline creation */
            VkRenderPass RenderPass{VK_NULL_HANDLE};

            /** Subpass index within the render pass */
            std::uint32_t Subpass{0};

            /** Compiled vertex shader bytecode */
            std::span<std::uint32_t const> VertexShader;

            /** Compiled fragment shader bytecode */
            std::span<std::uint32_t const> FragmentShader;

            /** Descriptor set layouts used by the pipeline */
            std::span<VkDescriptorSetLayout const> SetLayouts{};

            /** Vertex binding descriptions */
            std::span<VkVertexInputBindingDescription const> Bindings{};

            /** Vertex attribute descriptions */
            std::span<VkVertexInputAttributeDescription const> Attributes{};

            /** Push constant ranges */
            std::span<VkPushConstantRange const> PushConstants{};

            /** Primitive topology */
            VkPrimitiveTopology Topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

            /** Face culling mode */
            VkCullModeFlags CullMode{VK_CULL_MODE_BACK_BIT};

            /** Front face orientation */
            VkFrontFace FrontFace{VK_FRONT_FACE_COUNTER_CLOCKWISE};

            /** Additional pipeline creation flags */
            VkPipelineCreateFlags Flags{0};

            /** Optional pipeline cache */
            class PipelineCache* Cache{nullptr};
        };

        /** Create graphics pipeline for the specified render pass */
        void CreateGraphicsPipeline(std::shared_ptr<Device> const& DeviceModule, CreationArguments const& Arguments);

        /** Recreate the graphics pipeline destroying the old one */
        void RecreateGraphicsPipeline(std::shared_ptr<Device> const& DeviceModule, CreationArguments const& Arguments);

        [[nodiscard]] constexpr VkPipeline const& GetPipeline() const
        {
            return m_Pipeline;
        }

        [[nodiscard]] constexpr VkPipelineLayout const& GetPipelineLayout() const
        {
            return m_PipelineLayout;
        }

    private: /** Destroy the pipeline and layout */
        void ClearResources();
    };
} // namespace luvk
