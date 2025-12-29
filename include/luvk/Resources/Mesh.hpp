/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <array>
#include <memory>
#include <volk.h>
#include "luvk/Resources/Buffer.hpp"

namespace luvk
{
    class Material;
    class Device;
    class Memory;

    /**
     * Standard 3D transform components.
     */
    struct LUVK_API Transform
    {
        /** Position (X, Y, Z). */
        std::array<float, 3> Position{0.F, 0.F, 0.F};

        /** Euler rotation (Pitch, Yaw, Roll). */
        std::array<float, 3> Rotation{0.F, 0.F, 0.F};

        /** Scale (X, Y, Z). */
        std::array<float, 3> Scale{1.F, 1.F, 1.F};
    };

    /** Interface for all renderable meshes */
    class LUVK_API IMesh
    {
    protected:
        /** Pointer to the Device module. */
        Device* m_Device{nullptr};

        /** Pointer to the Memory module. */
        Memory* m_Memory{nullptr};

        /** The material used for rendering this mesh. */
        std::shared_ptr<Material> m_Material{};

        /** The world space transform of the mesh. */
        Transform m_Transform{};

    public:
        /**
         * Constructor.
         * @param Device Pointer to the Device module.
         * @param Memory Pointer to the Memory module.
         */
        explicit IMesh(Device* Device, Memory* Memory)
            : m_Device(Device),
              m_Memory(Memory) {}

        /** Virtual destructor. */
        virtual ~IMesh() = default;

        /** Sets the material for this mesh. */
        void SetMaterial(std::shared_ptr<Material> MaterialObj);

        /** Sets the transform for this mesh. */
        void SetTransform(const Transform& Transform);

        /** Get the shared pointer to the material. */
        [[nodiscard]] std::shared_ptr<Material> GetMaterial() const noexcept
        {
            return m_Material;
        }

        /**
         * Records the draw commands for the mesh.
         * @param CommandBuffer The command buffer to record into.
         */
        virtual void Render(VkCommandBuffer CommandBuffer) const = 0;
    };

    /** Standard Mesh with Index/Vertex Buffers */
    class LUVK_API Mesh : public IMesh
    {
    protected:
        /** The buffer holding vertex data. */
        std::shared_ptr<Buffer> m_VertexBuffer{};

        /** The buffer holding index data. */
        std::shared_ptr<Buffer> m_IndexBuffer{};

        /** The type of index (e.g., VK_INDEX_TYPE_UINT16). */
        VkIndexType m_IndexType{VK_INDEX_TYPE_UINT16};

        /** The number of indices to draw. */
        std::uint32_t m_IndexCount{0U};

        /** The number of vertices. */
        std::uint32_t m_VertexCount{0U};

    public:
        using IMesh::IMesh;

        /**
         * Uploads vertex data to the GPU buffer.
         * @param Data Span of raw bytes for vertices.
         * @param VertexCount The total number of vertices.
         */
        virtual void UploadVertices(std::span<const std::byte> Data, std::uint32_t VertexCount);

        /**
         * Uploads 16-bit index data to the GPU buffer.
         * @param Data Span of 16-bit indices.
         */
        virtual void UploadIndices(std::span<const std::uint16_t> Data);

        /**
         * Uploads 32-bit index data to the GPU buffer.
         * @param Data Span of 32-bit indices.
         */
        virtual void UploadIndices(std::span<const std::uint32_t> Data);

        /** Records the standard draw command (bind buffers, draw indexed). */
        void Render(VkCommandBuffer CommandBuffer) const override;
    };

    /** Mesh using Mesh Shaders (Task/Mesh NV/EXT) */
    class LUVK_API MeshShaderMesh : public IMesh
    {
    protected:
        /** Number of work groups to dispatch in the X dimension. */
        std::uint32_t m_DispatchX{1U};

        /** Number of work groups to dispatch in the Y dimension. */
        std::uint32_t m_DispatchY{1U};

        /** Number of work groups to dispatch in the Z dimension. */
        std::uint32_t m_DispatchZ{1U};

    public:
        using IMesh::IMesh;

        /** Dispatch task/mesh shader work groups */
        void Render(VkCommandBuffer CommandBuffer) const override;

        /** Get the X dimension dispatch count. */
        [[nodiscard]] constexpr std::uint32_t GetDispatchX() const
        {
            return m_DispatchX;
        }

        /** Get the Y dimension dispatch count. */
        [[nodiscard]] constexpr std::uint32_t GetDispatchY() const
        {
            return m_DispatchY;
        }

        /** Get the Z dimension dispatch count. */
        [[nodiscard]] constexpr std::uint32_t GetDispatchZ() const
        {
            return m_DispatchZ;
        }
    };
}
