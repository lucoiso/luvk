// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

namespace luvk
{
    class LUVKMODULE_API Debug : public IRenderModule
    {
        VkDebugUtilsMessengerEXT m_Messenger{VK_NULL_HANDLE};
        VkInstance m_Instance{VK_NULL_HANDLE};

    public:
        constexpr Debug() = default;

        ~Debug() override
        {
            Debug::ClearResources();
        }

        [[nodiscard]] UnorderedMap<std::string_view, Vector<std::string_view>> GetRequiredInstanceExtensions() const override
        {
            return ToExtensionMap("VK_LAYER_KHRONOS_validation", {VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
        }

    private:
        void InitializeDependencies(const std::shared_ptr<IRenderModule>& MainRenderer) override;
        void ClearResources() override;
    };
} // namespace luvk
