// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Core/Module.hpp"
#include "luvk/Core/Context.hpp"

luvk::Module::~Module() = default;

void luvk::Module::ConfigureInstance(InstanceRequirements&)
{
}

void luvk::Module::ConfigureDevice(DeviceRequirements&)
{
}

void luvk::Module::OnContextCreated(Context&)
{
}

void luvk::Module::OnContextDestroying(Context&)
{
}

luvk::MeshShaderModule::MeshShaderModule(const bool Required)
    : m_Required(Required)
{
}

void luvk::MeshShaderModule::ConfigureDevice(DeviceRequirements& Requirements)
{
    constexpr auto Extension = VK_EXT_MESH_SHADER_EXTENSION_NAME;
    auto&          Features  = m_Required ? Requirements.Features : Requirements.OptionalFeatures;
    auto&          Mesh      = Features.Get<VkPhysicalDeviceMeshShaderFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT, Extension);
    Mesh.meshShader          = VK_TRUE;
    Mesh.taskShader          = VK_TRUE;
    (m_Required ? Requirements.Extensions : Requirements.OptionalExtensions).emplace_back(Extension);
}

bool luvk::MeshShaderModule::IsAvailable(const Context& Ctx)
{
    const auto* const Mesh = Ctx.EnabledFeatures().Find<VkPhysicalDeviceMeshShaderFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT);
    return Mesh != nullptr && Mesh->meshShader != VK_FALSE && Mesh->taskShader != VK_FALSE &&
           Ctx.IsDeviceExtensionEnabled(VK_EXT_MESH_SHADER_EXTENSION_NAME);
}
