// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Types/Transform.hpp"

namespace luvk
{
    class Buffer;
    class Device;
    class Memory;
    class Material;

    class LUVKMODULE_API Mesh
    {
    public:
        struct InstanceInfo
        {
            Transform            XForm{};
            std::array<float, 4> Color{1.F, 1.F, 1.F, 1.F};
        };

    protected:
        std::shared_ptr<Buffer>   m_VertexBuffer{};
        std::shared_ptr<Buffer>   m_IndexBuffer{};
        std::shared_ptr<Buffer>   m_InstanceBuffer{};
        std::shared_ptr<Material> m_Material{};

        std::shared_ptr<Device> m_Device{};
        std::shared_ptr<Memory> m_Memory{};

        std::uint32_t m_IndexCount{0};
        std::uint32_t m_VertexCount{0};
        std::uint32_t m_InstanceCount{0};

        std::uint32_t m_DispatchX{1};
        std::uint32_t m_DispatchY{1};
        std::uint32_t m_DispatchZ{1};

    public:
        Mesh() = delete;
        explicit Mesh(const std::shared_ptr<Device>& Device, const std::shared_ptr<Memory>& Memory);
        virtual  ~Mesh() = default;

        void                                           SetMaterial(const std::shared_ptr<Material>& MaterialObj);
        [[nodiscard]] const std::shared_ptr<Material>& GetMaterial() const;

        void UploadVertices(std::span<const std::byte> Data, std::uint32_t VertexCount);
        void UploadIndices(std::span<const std::uint16_t> Data);
        void UploadIndices(std::span<const std::uint32_t> Data);

        void SetDispatchCount(std::uint32_t X, std::uint32_t Y, std::uint32_t Z);
        void UpdateInstances(std::span<const std::byte> Data, std::uint32_t Count);

        virtual void Draw(const VkCommandBuffer& CommandBuffer, std::span<const std::byte> PushConstants) const;
    };
} // namespace luvk
