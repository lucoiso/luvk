// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Modules/Debug.hpp"

#include "luvk/Libraries/VulkanHelpers.hpp"
#include "luvk/Modules/Renderer.hpp"

#include <cstdio>

VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback([[maybe_unused]] const VkDebugUtilsMessageSeverityFlagBitsEXT MessageSeverity,
                                                            [[maybe_unused]] const VkDebugUtilsMessageTypeFlagsEXT MessageType,
                                                            const VkDebugUtilsMessengerCallbackDataEXT* const CallbackData,
                                                            [[maybe_unused]] void* UserData)
{
    std::fprintf(stderr, "[%s] [%s]: %s\n", __func__, luvk::SeverityToString(MessageSeverity), CallbackData->pMessage);
    return VK_FALSE;
}

void luvk::Debug::InitializeDependencies(const std::shared_ptr<IRenderModule>& MainRenderer)
{
    m_Instance = std::dynamic_pointer_cast<Renderer>(MainRenderer)->GetInstance();

    constexpr auto Severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    constexpr auto Type = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    constexpr VkDebugUtilsMessengerCreateInfoEXT CreateInfo{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                                                            .pNext = nullptr,
                                                            .flags = 0,
                                                            .messageSeverity = Severity,
                                                            .messageType = Type,
                                                            .pfnUserCallback = &ValidationLayerDebugCallback,
                                                            .pUserData = nullptr};

    if (!LUVK_EXECUTE(vkCreateDebugUtilsMessengerEXT(m_Instance, &CreateInfo, nullptr, &m_Messenger)))
    {
        throw std::runtime_error("Failed to create the debug messenger.");
    }
}

void luvk::Debug::ClearResources()
{
    if (m_Messenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(m_Instance, m_Messenger, nullptr);
        m_Messenger = VK_NULL_HANDLE;
    }
    m_Instance = VK_NULL_HANDLE;
}
