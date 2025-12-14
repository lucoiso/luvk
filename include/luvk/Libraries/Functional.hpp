// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <algorithm>
#include <execution>
#include <type_traits>

namespace luvk
{
    template <typename... Args>
    consteval decltype(auto) Invoke(Args&&... Arguments)
    {
        return std::invoke(std::forward<Args>(Arguments)...);
    }

    constexpr bool Contains(const auto& Container, const auto& Item)
    {
        return std::find(std::execution::unseq, std::cbegin(Container), std::cend(Container), Item) != std::cend(Container);
    }
} // namespace luvk
