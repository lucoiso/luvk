/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include "luvk/Managers/Event.hpp"

namespace luvk
{
    /**
     * Interface for modules that provide an event system.
     */
    class LUVK_API IEventModule
    {
        /** The event graph instance associated with this module. */
        std::shared_ptr<EventGraph> m_EventGraph = std::make_shared<EventGraph>();

    public:
        /** Virtual destructor. */
        virtual ~IEventModule() = default;

        /**
         * Access the event graph associated with this module.
         * @return Reference to the EventGraph.
         */
        [[nodiscard]] EventGraph& GetEventSystem() const noexcept
        {
            return *m_EventGraph;
        }
    };
}
