/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Instance.hpp"
#include <vector>
#include "luvk/Libraries/VulkanHelpers.hpp"

using namespace luvk;

Instance::Instance()
{
    m_Extensions.FillExtensionsContainer();
}

void Instance::OnShutdown()
{
    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }

    IModule::OnShutdown();
}

bool Instance::Initialize(const InstanceCreationArguments& Arguments)
{
    m_Arguments = Arguments;

    const VkApplicationInfo AppInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName = std::data(Arguments.ApplicationName),
                                    .applicationVersion = Arguments.ApplicationVersion,
                                    .pEngineName = std::data(Arguments.EngineName),
                                    .engineVersion = Arguments.EngineVersion,
                                    .apiVersion = VK_API_VERSION_1_4};

    const auto Layers = m_Extensions.GetEnabledLayersNames();
    const auto Extensions = m_Extensions.GetEnabledExtensionsNames();

    const VkInstanceCreateInfo CreateInfo{.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                          .pApplicationInfo = &AppInfo,
                                          .enabledLayerCount = static_cast<std::uint32_t>(std::size(Layers)),
                                          .ppEnabledLayerNames = std::data(Layers),
                                          .enabledExtensionCount = static_cast<std::uint32_t>(std::size(Extensions)),
                                          .ppEnabledExtensionNames = std::data(Extensions)};

    if (!LUVK_EXECUTE(vkCreateInstance(&CreateInfo, nullptr, &m_Instance)))
    {
        return false;
    }

    volkLoadInstance(m_Instance);
    return true;
}
