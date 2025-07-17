// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** Debug.hpp definitions */

#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

#include <volk/volk.h>

namespace luvk
{
    /** Render module responsible for debugging messages */
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

        [[nodiscard]] std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredInstanceExtensions() const override
        {
            return ToExtensionMap("VK_LAYER_KHRONOS_validation", {VK_EXT_DEBUG_UTILS_EXTENSION_NAME});
        }

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const& DeviceModule) const noexcept override
        {
            return nullptr;
        }

        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

    private: /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources() override;
    };
} // namespace luvk
