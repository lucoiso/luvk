// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <vector>
#include <volk.h>

namespace luvk
{
    class Device;
    class PipelineCache;

    class LUVK_API Pipeline
    {
    public:
        enum class Type : std::uint8_t
        {
            Graphics,
            Compute,
            Mesh
        };

    protected:
        Type                             m_Type{Type::Graphics};
        VkPipelineLayout                 m_PipelineLayout{VK_NULL_HANDLE};
        VkPipeline                       m_Pipeline{VK_NULL_HANDLE};
        std::vector<VkPushConstantRange> m_PushConstants{};
        std::shared_ptr<Device>          m_DeviceModule{};

    public:
        Pipeline() = delete;
        explicit Pipeline(const std::shared_ptr<Device>& DeviceModule);

        ~Pipeline();

        struct CreationArguments
        {
            VkExtent2D                                         Extent{0U, 0U};
            std::span<const VkFormat>                          ColorFormats{};
            VkRenderPass                                       RenderPass{VK_NULL_HANDLE};
            std::uint32_t                                      Subpass{0};
            std::span<const std::uint32_t>                     VertexShader;
            std::span<const std::uint32_t>                     FragmentShader;
            std::span<const VkDescriptorSetLayout>             SetLayouts{};
            std::span<const VkVertexInputBindingDescription>   Bindings{};
            std::span<const VkVertexInputAttributeDescription> Attributes{};
            std::span<const VkPushConstantRange>               PushConstants{};
            VkPrimitiveTopology                                Topology{VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST};
            VkCullModeFlags                                    CullMode{VK_CULL_MODE_BACK_BIT};
            VkFrontFace                                        FrontFace{VK_FRONT_FACE_COUNTER_CLOCKWISE};
            bool                                               EnableDepthOp{true};
            VkPipelineCreateFlags                              Flags{0};
            PipelineCache*                                     Cache{};
        };

        struct ComputeCreationArguments
        {
            std::span<const std::uint32_t>         ComputeShader;
            std::span<const VkDescriptorSetLayout> SetLayouts{};
            std::span<const VkPushConstantRange>   PushConstants{};
            VkPipelineCreateFlags                  Flags{0};
            PipelineCache*                         Cache{};
        };

        struct MeshCreationArguments
        {
            VkExtent2D                             Extent{0U, 0U};
            std::span<const VkFormat>              ColorFormats{};
            VkRenderPass                           RenderPass{VK_NULL_HANDLE};
            std::uint32_t                          Subpass{0};
            std::span<const std::uint32_t>         TaskShader{};
            std::span<const std::uint32_t>         MeshShader;
            std::span<const std::uint32_t>         FragmentShader{};
            std::span<const VkDescriptorSetLayout> SetLayouts{};
            std::span<const VkPushConstantRange>   PushConstants{};
            VkCullModeFlags                        CullMode{VK_CULL_MODE_BACK_BIT};
            VkFrontFace                            FrontFace{VK_FRONT_FACE_COUNTER_CLOCKWISE};
            bool                                   EnableDepthOp{true};
            VkPipelineCreateFlags                  Flags{0};
            PipelineCache*                         Cache{};
        };

        void CreateGraphicsPipeline(const CreationArguments& Arguments);
        void RecreateGraphicsPipeline(const CreationArguments& Arguments);

        void CreateComputePipeline(const ComputeCreationArguments& Arguments);
        void RecreateComputePipeline(const ComputeCreationArguments& Arguments);

        void CreateMeshPipeline(const MeshCreationArguments& Arguments);
        void RecreateMeshPipeline(const MeshCreationArguments& Arguments);

        [[nodiscard]] constexpr VkPipeline GetPipeline() const noexcept
        {
            return m_Pipeline;
        }

        [[nodiscard]] constexpr VkPipelineLayout GetPipelineLayout() const noexcept
        {
            return m_PipelineLayout;
        }

        [[nodiscard]] constexpr std::span<const VkPushConstantRange> GetPushConstants() const noexcept
        {
            return m_PushConstants;
        }

        [[nodiscard]] constexpr Type GetType() const noexcept
        {
            return m_Type;
        }

        [[nodiscard]] constexpr VkPipelineBindPoint GetBindPoint() const noexcept
        {
            return m_Type == Type::Compute
                       ? VK_PIPELINE_BIND_POINT_COMPUTE
                       : VK_PIPELINE_BIND_POINT_GRAPHICS;
        }

    protected:
        void Clear();

    private:
        void SetupPipelineLayout(std::span<const VkDescriptorSetLayout> SetLayouts, std::span<const VkPushConstantRange> PushConstants);
    };
} // namespace luvk
