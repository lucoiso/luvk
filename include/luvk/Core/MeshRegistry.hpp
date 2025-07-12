// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/IRenderModule.hpp"
#include "luvk/Core/Buffer.hpp"
#include "luvk/Core/DescriptorSet.hpp"
#include "luvk/Core/DescriptorPool.hpp"
#include "luvk/Core/Pipeline.hpp"
#include "luvk/Types/MeshEntry.hpp"
#include "luvk/Types/Transform.hpp"
#include <iterator>

#include <array>
#include <span>

#include <memory>
#include <vector>

namespace luvk
{
    /** Render module responsible for storing mesh data */
    class Memory;
    class Device;
    class Image;
    class Sampler;

    class LUVKMODULE_API MeshRegistry : public IRenderModule
    {
    public:
    private: /** Stored mesh entries */
        std::vector<MeshEntry> m_Meshes{};

        /** Memory allocator used by meshes */
        std::shared_ptr<Memory> m_MemoryModule{};

    public:
        constexpr MeshRegistry() = default;
        ~MeshRegistry() override = default;

        /** Initialize the registry with the memory module */
        void Initialize(std::shared_ptr<Memory> const& MemoryModule);

        /** Register a mesh entry returning its index */
        struct InstanceInfo
        {
            Transform XForm{};
            std::array<float, 4> Color{1.F, 1.F, 1.F, 1.F};
        };

        /** Register a mesh entry. Descriptor resources are optional. */
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

        /** Remove a mesh from the registry */
        bool RemoveMesh(std::size_t MeshIndex);

        /** Change the pipeline used by a mesh */
        void SetPipeline(std::size_t MeshIndex, std::shared_ptr<Pipeline> const& PipelineModule) const;

        /** Update uniform buffer contents for a mesh */
        void UpdateUniform(std::size_t MeshIndex, std::span<std::byte const> Data);

        /** Create the instance buffer */
        void CreateInstanceBuffer(MeshEntry& Entry, std::span<InstanceInfo const> Instances) const;

        /** Update per-instance data for a mesh */
        void UpdateInstances(std::size_t MeshIndex, std::span<InstanceInfo const> Instances);

        /** Get the list of registered meshes */
        [[nodiscard]] constexpr std::span<MeshEntry const> GetMeshes() const
        {
            return {std::data(m_Meshes), std::size(m_Meshes)};
        }

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const& DeviceModule) const noexcept override
        {
            return nullptr;
        }

        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

    private:
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;
        void ClearResources() override;
    };
} // namespace luvk
