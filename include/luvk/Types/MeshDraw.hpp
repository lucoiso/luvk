#pragma once

#include "luvk/Types/MeshEntry.hpp"
#include "luvk/Types/Material.hpp"
#include <volk/volk.h>
#include <array>

namespace luvk
{
    /** Record draw commands for a mesh Entry */
    static inline void RecordMeshCommands(const VkCommandBuffer Command, MeshEntry const& Entry)
    {
        if (Entry.InstanceCount == 0 && !Entry.UniformBuffer)
        {
            return;
        }

        auto const& Material = Entry.MaterialPtr;
        if (!Material || !Material->GetPipeline())
        {
            return;
        }

        auto const& Pipeline = Material->GetPipeline();
        VkPipelineBindPoint const BindPoint = Pipeline->GetBindPoint();

        if (BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
        {
            std::array<VkBuffer, 2> Buffers{Entry.VertexBuffer->GetHandle(), VK_NULL_HANDLE};
            constexpr std::array<VkDeviceSize, 2> Offsets{0, 0};
            std::uint32_t bindingCount = 1U;

            if (Entry.InstanceBuffer)
            {
                Buffers[1] = Entry.InstanceBuffer->GetHandle();
                bindingCount = 2U;
            }

            vkCmdBindVertexBuffers(Command, 0, bindingCount, Buffers.data(), Offsets.data());
            vkCmdBindIndexBuffer(Command, Entry.IndexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT16);
        }

        vkCmdBindPipeline(Command, BindPoint, Pipeline->GetPipeline());

        if (auto const& Descriptor = Material->GetDescriptor())
        {
            vkCmdBindDescriptorSets(Command, BindPoint, Pipeline->GetPipelineLayout(), 0, 1, &Descriptor->GetHandle(), 0, nullptr);
        }

        if (!std::empty(Entry.UniformCache))
        {
            VkShaderStageFlags const Stages = Pipeline->GetType() == Pipeline::Type::Compute
                                                   ? VK_SHADER_STAGE_COMPUTE_BIT
                                                   : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

            vkCmdPushConstants(Command, Pipeline->GetPipelineLayout(), Stages, 0, static_cast<std::uint32_t>(Entry.UniformCache.size()), Entry.UniformCache.data());
        }

        switch (Pipeline->GetType())
        {
            case Pipeline::Type::Compute:
            {
                vkCmdDispatch(Command, 1, 1, 1);
                break;
            }
            case Pipeline::Type::Mesh:
            {
                vkCmdDrawMeshTasksEXT(Command, Entry.IndexCount, 1, 1);
                break;
            }
            case Pipeline::Type::Graphics:
            default:
            {
                std::uint32_t const drawInstances = Entry.InstanceBuffer ? Entry.InstanceCount : 1U;
                vkCmdDrawIndexed(Command, Entry.IndexCount, drawInstances, 0, 0, 0);
                break;
            }
        }
    }
} // namespace luvk
