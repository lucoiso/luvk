// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Libraries/MeshDraw.hpp"
#include <volk/volk.h>
#include "luvk/Resources/DescriptorSet.hpp"
#include "luvk/Types/Material.hpp"
#include "luvk/Types/Mesh.hpp"

void luvk::RecordMeshCommands(const VkCommandBuffer& Command, const MeshEntry& Entry)
{
    if (Entry.InstanceCount == 0 && !Entry.UniformBuffer)
    {
        return;
    }

    const auto& Material = Entry.MaterialPtr;
    if (!Material || !Material->GetPipeline())
    {
        return;
    }

    const auto& Pipeline = Material->GetPipeline();
    const VkPipelineBindPoint BindPoint = Pipeline->GetBindPoint();

    if (BindPoint == VK_PIPELINE_BIND_POINT_GRAPHICS && Pipeline->GetType() == Pipeline::Type::Graphics)
    {
        Vector<VkBuffer> Buffers{};
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

        const Vector<VkDeviceSize> Offsets(std::size(Buffers), 0);

        vkCmdBindVertexBuffers(Command, 0, static_cast<std::int32_t>(std::size(Buffers)), std::data(Buffers), std::data(Offsets));
        if (Entry.IndexBuffer)
        {
            vkCmdBindIndexBuffer(Command, Entry.IndexBuffer->GetHandle(), 0, VK_INDEX_TYPE_UINT16);
        }
    }

    vkCmdBindPipeline(Command, BindPoint, Pipeline->GetPipeline());

    if (const auto& Descriptor = Material->GetDescriptor())
    {
        vkCmdBindDescriptorSets(Command, BindPoint, Pipeline->GetPipelineLayout(), 0, 1, &Descriptor->GetHandle(), 0, nullptr);
    }

    const Pipeline::Type PipelineType = Pipeline->GetType();

    if (!std::empty(Entry.UniformCache))
    {
        VkShaderStageFlags Stages{};

        if (const auto& Ranges = Pipeline->GetPushConstants();
            !std::empty(Ranges))
        {
            for (const auto& Range : Ranges)
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
            const std::uint32_t drawInstances = Entry.InstanceBuffer
                                                    ? Entry.InstanceCount
                                                    : 1U;
            vkCmdDrawIndexed(Command, Entry.IndexCount, drawInstances, 0, 0, 0);
            break;
        }
    }
}
