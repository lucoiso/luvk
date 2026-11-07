// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include "luvk/Core/Module.hpp"
#include "luvk/Core/Common.hpp"
#include "luvk/Resources/Buffer.hpp"

namespace luvk
{
    class Context;

    /** Enables hardware ray tracing through ray queries (VK_KHR_acceleration_structure + VK_KHR_ray_query): rays are traced from
     *  any shader stage (typically compute) against the acceleration structures exposed in the bindless set at binding 3. */
    class LUVK_API RayTracingModule final : public Module
    {
        /** True when the ray tracing extensions and features are mandatory device requirements (optional otherwise). */
        bool m_Required;

    public:
        /** Constructor. */
        explicit RayTracingModule(bool Required = false);

        /** Requests the ray tracing extensions and features: mandatory when constructed with Required = true, optional otherwise. */
        void ConfigureDevice(DeviceRequirements& Requirements) override;

        /** True when the device was created with ray query support. */
        [[nodiscard]] static bool IsAvailable(const Context& Ctx);
    };

    /** Geometry description for a bottom-level acceleration structure build. */
    struct BottomLevelGeometry
    {
        /** xyz per vertex (tightly packed floats). */
        std::span<const float> Positions;

        /** Optional; triangle list. */
        std::span<const std::uint32_t> Indices;

        /** True to flag the geometry as opaque (any-hit shaders are skipped). */
        bool Opaque = true;
    };

    /** One instance of a bottom-level structure inside a top-level build. */
    struct TopLevelInstance
    {
        /** Bottom-level structure instanced by this record (must be built) and referenced through its device address. */
        const class AccelerationStructure* BottomLevel = nullptr;

        /** Row-major 3x4. */
        std::array<float, 12> Transform = {1.F, 0.F, 0.F, 0.F, 0.F, 1.F, 0.F, 0.F, 0.F, 0.F, 1.F, 0.F};

        /** Custom index visible to ray shaders (instanceCustomIndex, 24 bits). */
        std::uint32_t CustomIndex = 0U;

        /** Visibility mask combined with ray masks (8 bits). */
        std::uint8_t Mask = 0xFFU;
    };

    /** Move-only RAII acceleration structure. Builds run synchronously on the graphics queue, typically at load time;
     *  destruction is deferred through the deletion queue and top-level structures are registered in the bindless heap (BindlessIndex()). */
    class LUVK_API AccelerationStructure
    {
        /** Owning Context used for retirement and bindless management. */
        Context* m_Context = nullptr;

        /** Backing buffer that stores the acceleration structure. */
        Buffer m_Storage = {};

        /** Vulkan acceleration structure handle. */
        VkAccelerationStructureKHR m_Handle = VK_NULL_HANDLE;

        /** Device address of the structure, referenced by top-level builds and shaders. */
        VkDeviceAddress m_Address = 0U;

        /** Bindless slot in the acceleration structure array (binding 3), or InvalidBindlessIndex. */
        std::uint32_t m_BindlessIndex = InvalidBindlessIndex;

    public:
        /** Default constructor. */
        AccelerationStructure() = default;

        /** Destructor. */
        ~AccelerationStructure();

        /** Non-copyable. */
        AccelerationStructure(const AccelerationStructure&)            = delete;

        /** Non-copyable. */
        AccelerationStructure& operator=(const AccelerationStructure&) = delete;

        /** Move-only. */
        AccelerationStructure(AccelerationStructure&& Other) noexcept;

        /** Move-only. */
        AccelerationStructure& operator=(AccelerationStructure&& Other) noexcept;

        /** Builds a bottom-level structure from triangle geometry: Positions must hold at least one triangle (xyz per vertex). */
        [[nodiscard]] static AccelerationStructure BuildBottomLevel(Context& Ctx, const BottomLevelGeometry& Geometry, std::string_view Name = {});

        /** Builds a top-level structure from instances of built bottom-level structures and registers it in the bindless heap. */
        [[nodiscard]] static AccelerationStructure BuildTopLevel(Context& Ctx, std::span<const TopLevelInstance> Instances, std::string_view Name = {});

        /** Retires the structure, its storage buffer and its bindless slot (deferred) and leaves this object empty. */
        void Reset() noexcept;

        /** True when a VkAccelerationStructureKHR was created. */
        [[nodiscard]] explicit operator bool() const noexcept { return m_Handle != VK_NULL_HANDLE; }

        /** Underlying VkAccelerationStructureKHR handle. */
        [[nodiscard]] VkAccelerationStructureKHR Handle() const noexcept { return m_Handle; }

        /** Device address of the structure. */
        [[nodiscard]] VkDeviceAddress            Address() const noexcept { return m_Address; }

        /** Bindless slot index, or InvalidBindlessIndex for bottom-level structures. */
        [[nodiscard]] std::uint32_t              BindlessIndex() const noexcept { return m_BindlessIndex; }

    private:
        /** Shared build path for both structure types: allocates the storage buffer, records the build and waits on the graphics queue. */
        static AccelerationStructure Build(Context&                                       Ctx,
                                           VkAccelerationStructureTypeKHR                 Type,
                                           const VkAccelerationStructureGeometryKHR&      Geometry,
                                           std::uint32_t                                  PrimitiveCount,
                                           std::string_view                               Name);
    };
}
