// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <iterator>
#include <memory>
#include <span>
#include "luvk/Module.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include "luvk/Types/MeshEntry.hpp"
#include "luvk/Types/Transform.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class Memory;
    class Device;
    class Image;
    class Sampler;

    class LUVKMODULE_API MeshRegistry : public IRenderModule
    {
        Vector<MeshEntry> m_Meshes{};
        std::shared_ptr<Device> m_DeviceModule{};
        std::shared_ptr<Memory> m_MemoryModule{};

    public:
        MeshRegistry() = delete;
        explicit MeshRegistry(const std::shared_ptr<Device>& DeviceModule, const std::shared_ptr<Memory>& MemoryModule);

        ~MeshRegistry() override
        {
            MeshRegistry::ClearResources();
        }

        struct InstanceInfo
        {
            Transform XForm{};
            std::array<float, 4> Color{1.F, 1.F, 1.F, 1.F};
        };

        [[nodiscard]] std::size_t RegisterMesh(const std::span<const std::byte>& Vertices,
                                               const std::span<const std::byte>& Indices,
                                               const VkDescriptorSetLayout& Layout,
                                               const std::shared_ptr<DescriptorPool>& Pool,
                                               const std::shared_ptr<Image>& TexImage,
                                               const std::shared_ptr<Sampler>& TexSampler,
                                               const std::shared_ptr<Buffer>& UniformBuffer,
                                               const std::span<InstanceInfo>& Instances,
                                               const std::shared_ptr<Pipeline>& PipelineObj,
                                               std::uint32_t TaskCount = 1);

        bool RemoveMesh(std::size_t MeshIndex);
        void SetPipeline(std::size_t MeshIndex, const std::shared_ptr<Pipeline>& PipelineObj) const;
        void UpdateUniform(std::size_t MeshIndex, const std::span<const std::byte>& Data);
        void CreateInstanceBuffer(MeshEntry& Entry, const std::span<InstanceInfo>& Instances) const;
        void UpdateInstances(std::size_t MeshIndex, const std::span<InstanceInfo>& Instances);

        [[nodiscard]] constexpr std::span<const MeshEntry> GetMeshes() const
        {
            return {std::data(m_Meshes), std::size(m_Meshes)};
        }

    private:
        void ClearResources() override;
    };
} // namespace luvk
