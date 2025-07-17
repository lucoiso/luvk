// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** Functional.hpp definitions */

#include <execution>
#include <array>
#include <type_traits>

namespace luvk
{
    /** Invoke a callable in a constexpr context */
    template <typename... Args>
    consteval decltype(auto) Invoke(Args&&... Arguments)
    {
        return std::invoke(std::forward<Args>(Arguments)...);
    }

    /** Check if a container contains a value */
    constexpr bool Contains(auto const& Container, auto const& Item)
    {
        return std::find(std::execution::unseq, std::cbegin(Container), std::cend(Container), Item) != std::cend(Container);
    }
} // namespace luvk
