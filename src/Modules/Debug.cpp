/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/Debug.hpp"
#include <iostream>
#include "luvk/Interfaces/IServiceLocator.hpp"
#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Instance.hpp"

using namespace luvk;

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT Severity,
                                                    VkDebugUtilsMessageTypeFlagsEXT              Type,
                                                    const VkDebugUtilsMessengerCallbackDataEXT*  CallbackData,
                                                    void*                                        UserData)
{
    std::cerr << "[Validation Layer]: " << CallbackData->pMessage << std::endl;
    return VK_FALSE;
}

void Debug::OnInitialize(IServiceLocator* ServiceLocator)
{
    m_ServiceLocator        = ServiceLocator;
    const auto* InstanceMod = m_ServiceLocator->GetModule<Instance>();

    if (!InstanceMod || !vkCreateDebugUtilsMessengerEXT)
    {
        return;
    }

    constexpr auto SeverityFlags = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    constexpr auto TypeFlags = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;

    constexpr VkDebugUtilsMessengerCreateInfoEXT CreateInfo{.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                                                            .messageSeverity = SeverityFlags,
                                                            .messageType     = TypeFlags,
                                                            .pfnUserCallback = DebugCallback};

    if (!LUVK_EXECUTE(vkCreateDebugUtilsMessengerEXT(InstanceMod->GetHandle(), &CreateInfo, nullptr, &m_Messenger)))
    {
        throw std::runtime_error("Failed to create debug messenger");
    }

    IModule::OnInitialize(ServiceLocator);
}

void Debug::OnShutdown()
{
    if (m_Messenger != VK_NULL_HANDLE && m_ServiceLocator)
    {
        if (const auto* InstanceMod = m_ServiceLocator->GetModule<Instance>())
        {
            vkDestroyDebugUtilsMessengerEXT(InstanceMod->GetHandle(), m_Messenger, nullptr);
            m_Messenger = VK_NULL_HANDLE;
        }
    }

    IModule::OnShutdown();
}
