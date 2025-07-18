// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <iterator>
#include <memory>
#include <span>
#include "luvk/Types/Vector.hpp"
#include "luvk/Module.hpp"
#include "luvk/Modules/DescriptorPool.hpp"
#include "luvk/Resources/Buffer.hpp"
#include "luvk/Resources/DescriptorSet.hpp"
#include "luvk/Resources/Pipeline.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Types/MeshEntry.hpp"
#include "luvk/Types/Transform.hpp"

namespace luvk
{
    class Memory;
    class Device;
    class Image;
    class Sampler;

    class LUVKMODULE_API MeshRegistry : public IRenderModule
    {
        luvk::Vector<MeshEntry> m_Meshes{};
        std::shared_ptr<Memory> m_MemoryModule{};

    public:
        constexpr MeshRegistry() = default;
        ~MeshRegistry() override = default;

        void Initialize(std::shared_ptr<Memory> const& MemoryModule);

        struct InstanceInfo
        {
            Transform XForm{};
            std::array<float, 4> Color{1.F, 1.F, 1.F, 1.F};
        };

        [[nodiscard]] std::size_t RegisterMesh(std::span<std::byte const> Vertices,
                                               std::span<std::byte const> Indices,
                                               VkDescriptorSetLayout Layout,
                                               std::shared_ptr<DescriptorPool> const& Pool,
                                               std::shared_ptr<Image> const& TexImage,
                                               std::shared_ptr<Sampler> const& TexSampler,
                                               std::shared_ptr<Buffer> const& UniformBuffer,
                                               std::span<InstanceInfo const> Instances,
                                               std::shared_ptr<Pipeline> const& PipelineModule,
                                               std::shared_ptr<Device> const& DeviceModule,
                                               std::uint32_t TaskCount = 1);

        bool RemoveMesh(std::size_t MeshIndex);
        void SetPipeline(std::size_t MeshIndex, std::shared_ptr<Pipeline> const& PipelineModule) const;
        void UpdateUniform(std::size_t MeshIndex, std::span<std::byte const> Data);
        void CreateInstanceBuffer(MeshEntry& Entry, std::span<InstanceInfo const> Instances) const;
        void UpdateInstances(std::size_t MeshIndex, std::span<InstanceInfo const> Instances);

        [[nodiscard]] constexpr std::span<MeshEntry const> GetMeshes() const
        {
            return {std::data(m_Meshes), std::size(m_Meshes)};
        }

    private:
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override;
        void ClearResources() override;
    };
} // namespace luvk
