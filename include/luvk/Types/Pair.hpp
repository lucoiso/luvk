// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstddef>
#include <tuple>
#include <utility>
#include <type_traits>
#include "luvk/Module.hpp"

namespace luvk
{
    template <typename FirstType, typename SecondType>
    struct LUVKMODULE_API Pair
    {
        FirstType First{};
        SecondType Second{};
    };
} // namespace luvk

namespace std
{
    template <typename FirstType, typename SecondType>
    struct tuple_size<luvk::Pair<FirstType, SecondType>> : std::integral_constant<std::size_t, 2>
    {
    };

    template <std::size_t Idx, typename FirstType, typename SecondType>
    struct tuple_element<Idx, luvk::Pair<FirstType, SecondType>>
    {
        static_assert(Idx < 2);
        using type = std::conditional_t<Idx == 0, FirstType, SecondType>;
    };
} // namespace std

namespace luvk
{
    template <std::size_t Idx, typename FirstType, typename SecondType>
    constexpr std::tuple_element_t<Idx, luvk::Pair<FirstType, SecondType>>& get(luvk::Pair<FirstType, SecondType>& P) noexcept
    {
        if constexpr (Idx == 0)
        {
            return P.First;
        }
        else
        {
            return P.Second;
        }
    }

    template <std::size_t Idx, typename FirstType, typename SecondType>
    constexpr std::tuple_element_t<Idx, luvk::Pair<FirstType, SecondType>> const& get(luvk::Pair<FirstType, SecondType> const& P) noexcept
    {
        if constexpr (Idx == 0)
        {
            return P.First;
        }
        else
        {
            return P.Second;
        }
    }

    template <std::size_t Idx, typename FirstType, typename SecondType>
    constexpr std::tuple_element_t<Idx, luvk::Pair<FirstType, SecondType>>&& get(luvk::Pair<FirstType, SecondType>&& P) noexcept
    {
        if constexpr (Idx == 0)
        {
            return std::move(P.First);
        }
        else
        {
            return std::move(P.Second);
        }
    }
} // namespace luvk
