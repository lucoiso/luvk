// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/Event.hpp"
#include <memory>

namespace luvk
{
    /** Interface for a render module */
    class LUVKMODULE_API IRenderModule
    {
        EventGraph m_EventGraph{};

    public:
        constexpr IRenderModule() = default;
        virtual ~IRenderModule() = default;

        /** Get the event management system */
        [[nodiscard]] inline EventGraph& GetEventSystem()
        {
            return m_EventGraph;
        }

        /** Initialize the dependencies of this module */
        virtual void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) = 0;

        /** Clear the resources of this module */
        virtual void ClearResources(IRenderModule* MainRenderer) = 0;
    };
} // namespace luvk