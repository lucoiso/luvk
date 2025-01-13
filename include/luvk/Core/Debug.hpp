// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <volk/volk.h>

namespace luvk
{
    /** Render module responsible for debugging messages */
    class LUVKMODULE_API Debug : public IRenderModule
    {
        VkDebugUtilsMessengerEXT m_Messenger{VK_NULL_HANDLE};

    public:
        constexpr Debug() = default;
        ~Debug() override = default;

    private:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources(IRenderModule* MainRenderer) override;
    };
} // namespace luvk