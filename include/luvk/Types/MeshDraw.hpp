// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** MeshDraw.hpp definitions */

#include "luvk/Types/MeshEntry.hpp"
#include "luvk/Types/Material.hpp"
#include <volk/volk.h>
#include <array>

namespace luvk
{
    /** Record draw commands for a mesh Entry */
    static inline void RecordMeshCommands(const VkCommandBuffer& Command, MeshEntry const& Entry)
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

        if (BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS && Pipeline->GetType() == Pipeline::Type::Graphics)
        {
            std::vector<VkBuffer> Buffers{};
            Buffers.reserve(2U);

            if (Entry.VertexBuffer)
            {
                Buffers.push_back(Entry.VertexBuffer->GetHandle());
            }

            if (Entry.InstanceBuffer)
            {
                Buffers.push_back(Entry.InstanceBuffer->GetHandle());
            }

            if (std::empty(Buffers))
            {
                return;
            }

            const std::vector<VkDeviceSize> Offsets(std::size(Buffers), 0);

            vkCmdBindVertexBuffers(Command, 0, static_cast<std::int32_t>(std::size(Buffers)), std::data(Buffers), std::data(Offsets));
            if (Entry.IndexBuffer)
            {
                vkCmdBindIndexBuffer(Command, Entry.IndexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT16);
            }
        }

        vkCmdBindPipeline(Command, BindPoint, Pipeline->GetPipeline());

        if (auto const& Descriptor = Material->GetDescriptor())
        {
            vkCmdBindDescriptorSets(Command, BindPoint, Pipeline->GetPipelineLayout(), 0, 1, &Descriptor->GetHandle(), 0, nullptr);
        }

        const Pipeline::Type PipelineType = Pipeline->GetType();

        if (!std::empty(Entry.UniformCache))
        {
            VkShaderStageFlags Stages{};
            auto const& Ranges = Pipeline->GetPushConstants();
            if (!std::empty(Ranges))
            {
                for (auto const& Range : Ranges)
                {
                    Stages |= Range.stageFlags;
                }
            }
            else
            {
                Stages = PipelineType == Pipeline::Type::Compute
                             ? VK_SHADER_STAGE_COMPUTE_BIT
                             : PipelineType == Pipeline::Type::Mesh
                                   ? VK_SHADER_STAGE_MESH_BIT_EXT
                                   : VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
            }

            vkCmdPushConstants(Command,
                               Pipeline->GetPipelineLayout(),
                               Stages,
                               0,
                               static_cast<std::uint32_t>(std::size(Entry.UniformCache)),
                               std::data(Entry.UniformCache));
        }

        switch (PipelineType)
        {
        case Pipeline::Type::Compute:
            {
                vkCmdDispatch(Command, Entry.DispatchX, Entry.DispatchY, Entry.DispatchZ);
                break;
            }
        case Pipeline::Type::Mesh:
            {
                if (vkCmdDrawMeshTasksEXT)
                {
                    vkCmdDrawMeshTasksEXT(Command, Entry.DispatchX, Entry.DispatchY, Entry.DispatchZ);
                }
                break;
            }
        case Pipeline::Type::Graphics:
        default:
            {
                std::uint32_t const drawInstances = Entry.InstanceBuffer
                                                        ? Entry.InstanceCount
                                                        : 1U;
                vkCmdDrawIndexed(Command, Entry.IndexCount, drawInstances, 0, 0, 0);
                break;
            }
        }
    }
} // namespace luvk
