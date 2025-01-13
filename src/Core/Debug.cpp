// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Debug.hpp"

#include "luvk/Core/Renderer.hpp"

#include <iostream>

VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback([[maybe_unused]] VkDebugUtilsMessageSeverityFlagBitsEXT const MessageSeverity,
                                                            [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT const        MessageType,
                                                            VkDebugUtilsMessengerCallbackDataEXT const* const             CallbackData,
                                                            [[maybe_unused]] void*                                        UserData)
{
    std::clog << "[" << __func__ << "]: Message: " << CallbackData->pMessage << '\n';
    return VK_FALSE;
}

void luvk::Debug::InitializeDependencies(std::shared_ptr<IRenderModule> const &MainRenderer)
{
    VkInstance const& Instance = static_cast<luvk::Renderer*>(MainRenderer.get())->GetInstance();

    constexpr auto Severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    constexpr auto Type = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT     |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT  |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;

    VkDebugUtilsMessengerCreateInfoEXT const CreateInfo{.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                                                        .pNext = nullptr,
                                                        .flags = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT,
                                                        .messageSeverity = Severity,
                                                        .messageType = Type,
                                                        .pfnUserCallback = &ValidationLayerDebugCallback,
                                                        .pUserData = nullptr};

    if (vkCreateDebugUtilsMessengerEXT(Instance, &CreateInfo, nullptr, &m_Messenger) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create the debug messenger.");
    }
}

void luvk::Debug::ClearResources(IRenderModule *MainRenderer)
{
    VkInstance const& Instance = static_cast<luvk::Renderer*>(MainRenderer)->GetInstance();

    if (m_Messenger != VK_NULL_HANDLE)
    {
        vkDestroyDebugUtilsMessengerEXT(Instance, m_Messenger, nullptr);
        m_Messenger = VK_NULL_HANDLE;
    }
}
