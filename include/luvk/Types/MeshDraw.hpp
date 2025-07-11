#pragma once

#include "luvk/Types/MeshEntry.hpp"
#include "luvk/Types/Material.hpp"
#include <volk/volk.h>
#include <array>

namespace luvk
{
    /** Record draw commands for a mesh entry */
    inline void RecordMeshCommands(VkCommandBuffer cmd, MeshEntry const& entry)
    {
        if (entry.InstanceCount == 0 && !entry.UniformBuffer)
        {
            return;
        }

        auto const& material = entry.MaterialPtr;
        if (!material || !material->GetPipeline())
        {
            return;
        }

        auto const& pipe = material->GetPipeline();
        VkPipelineBindPoint const bindPoint = pipe->GetBindPoint();

        if (bindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS)
        {
            std::array<VkBuffer, 2> buffers{entry.VertexBuffer->GetHandle(), VK_NULL_HANDLE};
            constexpr std::array<VkDeviceSize, 2> offsets{0, 0};
            std::uint32_t bindingCount = 1U;

            if (entry.InstanceBuffer)
            {
                buffers[1] = entry.InstanceBuffer->GetHandle();
                bindingCount = 2U;
            }

            vkCmdBindVertexBuffers(cmd, 0, bindingCount, buffers.data(), offsets.data());
            vkCmdBindIndexBuffer(cmd, entry.IndexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT16);
        }

        vkCmdBindPipeline(cmd, bindPoint, pipe->GetPipeline());

        if (auto const& descriptor = material->GetDescriptor())
        {
            vkCmdBindDescriptorSets(cmd, bindPoint, pipe->GetPipelineLayout(), 0, 1, &descriptor->GetHandle(), 0, nullptr);
        }

        if (!entry.UniformCache.empty())
        {
            VkShaderStageFlags const stages = pipe->GetType() == Pipeline::Type::Compute
                                                   ? VK_SHADER_STAGE_COMPUTE_BIT
                                                   : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            vkCmdPushConstants(cmd, pipe->GetPipelineLayout(), stages, 0, static_cast<std::uint32_t>(entry.UniformCache.size()), entry.UniformCache.data());
        }

        switch (pipe->GetType())
        {
        case Pipeline::Type::Compute:
            vkCmdDispatch(cmd, 1, 1, 1);
            break;
        case Pipeline::Type::Mesh:
            vkCmdDrawMeshTasksEXT(cmd, entry.IndexCount, 1, 1);
            break;
        case Pipeline::Type::Graphics:
        default:
            {
                std::uint32_t const drawInstances = entry.InstanceBuffer ? entry.InstanceCount : 1U;
                vkCmdDrawIndexed(cmd, entry.IndexCount, drawInstances, 0, 0, 0);
            }
            break;
        }
    }
} // namespace luvk
