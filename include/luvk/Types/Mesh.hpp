// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <memory>
#include <span>
#include <vector>
#include <volk.h>
#include "luvk/Module.hpp"
#include "luvk/Constants/Rendering.hpp"
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
        std::array<std::shared_ptr<Buffer>, Constants::ImageCount> m_VertexBuffers{};
        std::array<std::shared_ptr<Buffer>, Constants::ImageCount> m_IndexBuffers{};
        std::array<std::shared_ptr<Buffer>, Constants::ImageCount> m_InstanceBuffers{};
        std::array<std::shared_ptr<Buffer>, Constants::ImageCount> m_UniformBuffers{};
        std::shared_ptr<Material>                                  m_Material{};

        std::vector<std::byte> m_PushConstantData{};

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

        void SetMaterial(const std::shared_ptr<Material>& MaterialObj)
        {
            m_Material = MaterialObj;
        }

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
        virtual void Render(VkCommandBuffer CommandBuffer, std::uint32_t CurrentFrame) const;
    };
}
