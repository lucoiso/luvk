// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** Array.hpp definitions */

#include "luvk/Module.hpp"

#include <array>
#include <execution>

namespace luvk
{
    /** Minimal array wrapper with size tracking */
    template <typename Type, std::size_t Capacity = 512U>
    struct LUVKMODULE_API Array
    {
        using value_type = Type;

        /** Storage for elements */
        std::array<Type, Capacity> Data{};

        /** Number of valid elements */
        std::size_t Size{0U};

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
