/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <volk.h>
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Renderer;

    class LUVK_API Debug : public IRenderModule
                         , public IExtensionsModule
    {
    protected:
        VkDebugUtilsMessengerEXT m_Messenger{VK_NULL_HANDLE};
        std::weak_ptr<Renderer>  m_RendererModule{};

    public:
        Debug() = delete;
        explicit Debug(const std::shared_ptr<Renderer>& RendererModule);

        ~Debug() override
        {
            Debug::ClearResources();
        }

        [[nodiscard]] ExtensionMap GetInstanceExtensions() const noexcept override
        {
            return {{"VK_LAYER_KHRONOS_validation",
                     {VK_EXT_DEBUG_UTILS_EXTENSION_NAME}}};
        }

    protected:
        void InitializeResources() override;
        void ClearResources() override;
    };
}
