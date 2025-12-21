// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <volk.h>
#include "luvk/Module.hpp"
#include "luvk/Types/Transform.hpp"
#include "luvk/Types/Vector.hpp"

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
            Transform             XForm{};
            luvk::Array<float, 4> Color{1.F, 1.F, 1.F, 1.F};
        };

    protected:
        Vector<std::shared_ptr<Buffer>> m_VertexBuffers{};
        Vector<std::shared_ptr<Buffer>> m_IndexBuffers{};
        Vector<std::shared_ptr<Buffer>> m_InstanceBuffers{};
        Vector<std::shared_ptr<Buffer>> m_UniformBuffers{};
        std::shared_ptr<Material>       m_Material{};

        Vector<std::byte> m_PushConstantData{};

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

        void SetMaterial(const std::shared_ptr<Material>& MaterialObj);

        [[nodiscard]] std::shared_ptr<Material> GetMaterial() const noexcept
        {
            return m_Material;
        }

        void UploadVertices(std::span<const std::byte> Data, std::uint32_t VertexCount, std::uint32_t FrameIndex);
        void UploadIndices(std::span<const std::uint16_t> Data, std::uint32_t FrameIndex);
        void UploadIndices(std::span<const std::uint32_t> Data, std::uint32_t FrameIndex);
        void UpdateInstances(std::span<const std::byte> Data, std::uint32_t Count, std::uint32_t FrameIndex);
        void UpdateUniformBuffer(std::span<const std::byte> Data, std::uint32_t FrameIndex);

        void UploadVerticesToAll(std::span<const std::byte> Data, std::uint32_t VertexCount);
        void UploadIndicesToAll(std::span<const std::uint16_t> Data);
        void UploadIndicesToAll(std::span<const std::uint32_t> Data);

        void SetDispatchCount(std::uint32_t X, std::uint32_t Y, std::uint32_t Z);
        void SetPushConstantData(std::span<const std::byte> Data);

        virtual void Tick(float DeltaTime);
        virtual void Render(const VkCommandBuffer& CommandBuffer, std::uint32_t CurrentFrame) const;
    };
}
