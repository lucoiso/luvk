// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Core/FeatureChain.hpp"

namespace luvk
{
    class Context;

    /**
     * Extension point of the framework.
     *
     * A module declares what it needs (instance/device extensions and typed feature structures) before the
     * device is created, and gets lifecycle callbacks afterwards. Modules are destroyed in reverse order.
     *
     * Example: a mesh shading module adds "VK_EXT_mesh_shader" and sets
     * Requirements.Features.Get<VkPhysicalDeviceMeshShaderFeaturesEXT>(sType, "VK_EXT_mesh_shader").meshShader.
     */
    class LUVK_API Module
    {
    public:
        /** Destructor. */
        virtual ~Module();

        /** Called before the instance is created: add instance extensions/layers to Requirements. */
        virtual void ConfigureInstance(InstanceRequirements& Requirements);

        /** Called before the device is created: add device extensions and feature structures to Requirements. */
        virtual void ConfigureDevice(DeviceRequirements& Requirements);

        /** Called once the Context is fully initialized (device, queues, allocator ready). */
        virtual void OnContextCreated(Context& Ctx);

        /** Called before the Context destroys the device; modules must release what they own here. */
        virtual void OnContextDestroying(Context& Ctx);
    };

    /** Ready-made module enabling VK_EXT_mesh_shader (task + mesh shaders). */
    class LUVK_API MeshShaderModule final : public Module
    {
        /** Whether mesh shading is a hard requirement (true) or an optional feature (false). */
        bool m_Required;

    public:
        /** Constructor: Required selects between mandatory and optional registration of the extension/features. */
        explicit MeshShaderModule(bool Required = true);

        /** Adds VK_EXT_mesh_shader plus task/mesh shader features to Requirements (required or optional slot). */
        void ConfigureDevice(DeviceRequirements& Requirements) override;

        /** True when the device was created with task and mesh shader support. */
        [[nodiscard]] static bool IsAvailable(const Context& Ctx);
    };
}
