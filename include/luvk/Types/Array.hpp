// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <iterator>
#include "luvk/Module.hpp"

namespace luvk
{
    template <typename Type, std::size_t Capacity = 512U>
    struct LUVKMODULE_API Array
    {
        using value_type = Type;

        std::array<Type, Capacity> Data{};
        std::size_t                Size{0U};

        [[nodiscard]] constexpr std::size_t size() const noexcept
        {
            return Size;
        }

        [[nodiscard]] constexpr auto begin() noexcept
        {
            return std::begin(Data);
        }

        [[nodiscard]] constexpr auto end() noexcept
        {
            return std::next(std::begin(Data), static_cast<std::ptrdiff_t>(Size));
        }

        [[nodiscard]] constexpr auto begin() const noexcept
        {
            return std::cbegin(Data);
        }

        [[nodiscard]] constexpr auto end() const noexcept
        {
            return std::next(std::cbegin(Data), static_cast<std::ptrdiff_t>(Size));
        }
    };
} // namespace luvk
