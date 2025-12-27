// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Resources/Event.hpp"

namespace luvk
{
    class LUVK_API IEventModule
    {
        mutable EventGraph m_EventGraph{};

    public:
        [[nodiscard]] EventGraph& GetEventSystem() const noexcept
        {
            return m_EventGraph;
        }
    };
} // namespace luvk
