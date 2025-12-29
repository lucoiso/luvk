/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <volk.h>
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IModule.hpp"

namespace luvk
{
    /**
     * Module for setting up Vulkan Debug Utils Messenger for validation layer feedback.
     */
    class LUVK_API Debug : public IModule
                         , public IExtensionsModule
    {
        /** The Vulkan Debug Utils Messenger handle. */
        VkDebugUtilsMessengerEXT m_Messenger{VK_NULL_HANDLE};

        /** Pointer to the central service locator. */
        IServiceLocator* m_ServiceLocator{nullptr};

    public:
        /** Default destructor. */
        ~Debug() override = default;

        /** Called upon module initialization (sets up the messenger). */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /** Called upon module shutdown (destroys the messenger). */
        void OnShutdown() override;

        /** Get the required instance extensions (Debug Utils and Validation Layer). */
        [[nodiscard]] ExtensionMap GetInstanceExtensions() const noexcept override
        {
            return {{"VK_LAYER_KHRONOS_validation", {VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}};
        }
    };
}
