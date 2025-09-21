#pragma once

#include "luvk/Module.hpp"
#include "luvk/Resources/Event.hpp"

namespace luvk
{
    class LUVKMODULE_API IEventModule
    {
        mutable EventGraph m_EventGraph{};

    public:
        [[nodiscard]] EventGraph& GetEventSystem() const
        {
            return m_EventGraph;
        }
    };
} // namespace luvk
