/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <span>
#include <vector>
#include <volk.h>

namespace luvk
{
    class Device;

    /**
     * Wrapper for a Vulkan Pipeline and Pipeline Layout.
     */
    class LUVK_API Pipeline
    {
    public:
        /** Defines the type of pipeline (Graphics, Compute, or Mesh). */
        enum class Type : std::uint8_t
        {
            Graphics,
            Compute,
            Mesh
        };

    protected:
        /** The type of this pipeline. */
        Type m_Type{Type::Graphics};

        /** The Vulkan pipeline layout handle. */
        VkPipelineLayout m_PipelineLayout{VK_NULL_HANDLE};

        /** The Vulkan pipeline handle. */
        VkPipeline m_Pipeline{VK_NULL_HANDLE};

        /** Cached push constant ranges for the layout. */
        std::vector<VkPushConstantRange> m_PushConstants{};

        /** Cached shader stages that use push constants. */
        VkShaderStageFlags m_PushConstantStages{0};

        /** Pointer to the Device module for logical device handle. */
        Device* m_DeviceModule{nullptr};

    public:
        /** Pipelines cannot be default constructed. */
        Pipeline() = delete;

        /**
         * Constructor.
         * @param DeviceModule Pointer to the Device module.
         */
        explicit Pipeline(Device* DeviceModule);

        /** Destructor (destroys the pipeline and pipeline layout). */
        ~Pipeline();

        /**
         * Arguments for creating a graphics pipeline with Dynamic Rendering.
         */
        struct GraphicsCreationArguments
        {
            /** SPIR-V code for the vertex shader stage. */
            std::span<const std::uint32_t> VertexShader;

            /** SPIR-V code for the fragment shader stage. */
            std::span<const std::uint32_t> FragmentShader;

            /** Color attachment formats (for Dynamic Rendering). */
            std::span<const VkFormat> ColorFormats{};

            /** Depth attachment format (for Dynamic Rendering). */
            VkFormat DepthFormat{VK_FORMAT_UNDEFINED};

            /** Primitive type (e.g., triangle list). */
            VkPrimitiveTopology Topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};

            /** Face culling mode. */
            VkCullModeFlags CullMode{VK_CULL_MODE_BACK_BIT};

            /** Front face orientation. */
            VkFrontFace FrontFace{VK_FRONT_FACE_COUNTER_CLOCKWISE};

            /** Enable depth testing. */
            bool EnableDepthTest{true};

            /** Enable depth writing. */
            bool EnableDepthWrite{true};

            /** Descriptor set layouts for the pipeline layout. */
            std::span<const VkDescriptorSetLayout> SetLayouts{};

            /** Push constant ranges for the pipeline layout. */
            std::span<const VkPushConstantRange> PushConstants{};

            /** Vertex buffer binding descriptions. */
            std::span<const VkVertexInputBindingDescription> VertexBindings{};

            /** Vertex attribute descriptions. */
            std::span<const VkVertexInputAttributeDescription> VertexAttributes{};

            /** Optional debug name for the pipeline. */
            std::string_view Name{};
        };

        /**
         * Arguments for creating a mesh pipeline (Mesh/Task/Fragment).
         */
        struct MeshCreationArguments
        {
            /** SPIR-V code for the optional task shader stage. */
            std::span<const std::uint32_t> TaskShader{};

            /** SPIR-V code for the mesh shader stage. */
            std::span<const std::uint32_t> MeshShader;

            /** SPIR-V code for the optional fragment shader stage. */
            std::span<const std::uint32_t> FragmentShader{};

            /** Color attachment formats (for Dynamic Rendering). */
            std::span<const VkFormat> ColorFormats{};

            /** Depth attachment format (for Dynamic Rendering). */
            VkFormat DepthFormat{VK_FORMAT_UNDEFINED};

            /** Face culling mode. */
            VkCullModeFlags CullMode{VK_CULL_MODE_BACK_BIT};

            /** Front face orientation. */
            VkFrontFace FrontFace{VK_FRONT_FACE_COUNTER_CLOCKWISE};

            /** Enable depth testing. */
            bool EnableDepthTest{true};

            /** Enable depth writing. */
            bool EnableDepthWrite{true};

            /** Descriptor set layouts for the pipeline layout. */
            std::span<const VkDescriptorSetLayout> SetLayouts{};

            /** Push constant ranges for the pipeline layout. */
            std::span<const VkPushConstantRange> PushConstants{};

            /** Optional debug name for the pipeline. */
            std::string_view Name{};
        };

        /**
         * Arguments for creating a compute pipeline.
         */
        struct ComputeCreationArguments
        {
            /** SPIR-V code for the compute shader stage. */
            std::span<const std::uint32_t> ComputeShader;

            /** Descriptor set layouts for the pipeline layout. */
            std::span<const VkDescriptorSetLayout> SetLayouts{};

            /** Push constant ranges for the pipeline layout. */
            std::span<const VkPushConstantRange> PushConstants{};

            /** Optional debug name for the pipeline. */
            std::string_view Name{};
        };

        /**
         * Creates a Graphics Pipeline.
         * @param Arguments Configuration for the pipeline creation.
         */
        void CreateGraphicsPipeline(const GraphicsCreationArguments& Arguments);
        /**
         * Creates a Mesh Pipeline.
         * @param Arguments Configuration for the pipeline creation.
         */
        void CreateMeshPipeline(const MeshCreationArguments& Arguments);
        /**
         * Creates a Compute Pipeline.
         * @param Arguments Configuration for the pipeline creation.
         */
        void CreateComputePipeline(const ComputeCreationArguments& Arguments);

        /** Get the underlying VkPipeline handle. */
        [[nodiscard]] constexpr VkPipeline GetHandle() const noexcept
        {
            return m_Pipeline;
        }

        /** Get the underlying VkPipelineLayout handle. */
        [[nodiscard]] constexpr VkPipelineLayout GetLayout() const noexcept
        {
            return m_PipelineLayout;
        }

        /** Get the type of the pipeline. */
        [[nodiscard]] constexpr Type GetType() const noexcept
        {
            return m_Type;
        }

        /** Get the shader stages that use push constants for this pipeline. */
        [[nodiscard]] constexpr VkShaderStageFlags GetPushConstantStages() const noexcept
        {
            return m_PushConstantStages;
        }

        /** Get the Vulkan bind point (Graphics or Compute) for this pipeline. */
        [[nodiscard]] constexpr VkPipelineBindPoint GetBindPoint() const noexcept
        {
            if (m_Type == Type::Graphics || m_Type == Type::Mesh)
            {
                return VK_PIPELINE_BIND_POINT_GRAPHICS;
            }
            return VK_PIPELINE_BIND_POINT_COMPUTE;
        }
    };
}
