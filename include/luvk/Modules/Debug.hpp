// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Interfaces/IExtensionsModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Renderer;

    class LUVKMODULE_API Debug : public IRenderModule,
                                 public IExtensionsModule
    {
        VkDebugUtilsMessengerEXT m_Messenger{VK_NULL_HANDLE};
        std::shared_ptr<Renderer> m_RendererModule{};

    public:
        Debug() = delete;
        explicit Debug(const std::shared_ptr<Renderer>& RendererModule);

        ~Debug() override
        {
            Debug::ClearResources();
        }

        [[nodiscard]] ExtensionsMap GetInstanceExtensions() const override
        {
            return ToExtensionMap("VK_LAYER_KHRONOS_validation", {VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
        }

    protected:
        void InitializeResources() override;
        void ClearResources() override;
    };
} // namespace luvk
