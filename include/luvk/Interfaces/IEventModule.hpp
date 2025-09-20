#pragma once

#include "luvk/Module.hpp"
#include "luvk/Resources/Event.hpp"

namespace luvk
{
    class LUVKMODULE_API IEventModule
    {
        EventGraph m_EventGraph{};

    public:
        [[nodiscard]] EventGraph& GetEventSystem()
        {
            return m_EventGraph;
        }
    };
} // namespace luvk
