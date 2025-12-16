#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <iterator>
#include <type_traits>

namespace luvk
{
    template <typename Type, std::size_t Capacity = 256U>
    struct Array
    {
        using value_type = Type;

        std::array<Type, Capacity> Data;
        std::size_t                Size;

        constexpr Array() noexcept : Data{},
                                     Size(0U) {}

        constexpr Array(std::initializer_list<Type> Init)
            noexcept(std::is_nothrow_copy_constructible_v<Type>)
            : Data{},
              Size(std::size(Init))
        {
            assert(Size <= Capacity && "Initializer list exceeds Array capacity.");
            std::copy(std::begin(Init), std::end(Init), std::begin(Data));
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept
        {
            return Size;
        }

        [[nodiscard]] constexpr std::size_t size() noexcept
        {
            return Size;
        }

        [[nodiscard]] constexpr Type& at(std::size_t Pos)
        {
            return Data.at(Pos);
        }

        [[nodiscard]] constexpr const Type& at(std::size_t Pos) const
        {
            return Data.at(Pos);
        }

        constexpr void fill(Type&& Value)
        {
            return Data.fill(Value);
        }

        [[nodiscard]] constexpr auto data() noexcept
        {
            return std::data(Data);
        }

        [[nodiscard]] constexpr auto data() const noexcept
        {
            return std::data(Data);
        }

        [[nodiscard]] constexpr auto begin() noexcept
        {
            return std::begin(Data);
        }

        [[nodiscard]] constexpr auto begin() const noexcept
        {
            return std::cbegin(Data);
        }

        [[nodiscard]] constexpr auto end() const noexcept
        {
            return std::next(std::cbegin(Data), static_cast<std::ptrdiff_t>(Size));
        }

        [[nodiscard]] constexpr auto end() noexcept
        {
            return std::next(std::begin(Data), static_cast<std::ptrdiff_t>(Size));
        }
    };

    template <typename Type, typename... Args>
    Array(Type, Args...) -> Array<Type, 1 + sizeof...(Args)>;
}
