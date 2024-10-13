// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

#include <array>
#include <execution>
#include <span>
#include <xstring>

namespace luvk
{
    // Functions inspired by Jason Turner's implementations that can be found on:
    // https://github.com/lefticus/tools/blob/main/include/lefticus/tools/static_views.hpp

    template <auto Data>
    constexpr static auto &AsStatic = Data;

    template<typename Type, std::size_t Capacity = 1024U>
    struct LUVKMODULE_API Array
    {
        using value_type = Type;

        std::array<Type, Capacity> Data {};
        std::size_t Size { 0U };

        constexpr void Emplace(Type&& Item) noexcept
        {
            Data.at(Size) = Item;
            ++Size;
        }

        constexpr void Emplace(Type const& Item) noexcept
        {
            Data.at(Size) = Item;
            ++Size;
        }

        [[nodiscard]] constexpr bool Contains(Type const& Item) const noexcept
        {
            return std::find(std::execution::unseq,
                             std::cbegin(Data),
                             std::cend(Data),
                             Item) != std::cend(Data);
        }

        constexpr void clear() noexcept
        {
            std::for_each(std::execution::unseq,
                          std::begin(Data),
                          std::end(Data),
                          [] (Type& Iterator) { Iterator = Type {}; });

            Size = 0U;
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return Size == 0U;
        }

        [[nodiscard]] constexpr auto data() const noexcept
        {
            return std::data(Data);
        }

        [[nodiscard]] constexpr auto size() const noexcept
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

    constexpr auto GenerateArray(auto Getter)
    {
        auto GeneratedContainer = Getter();
        constexpr auto& StaticContainer = AsStatic<GeneratedContainer>;

        using InputType = typename std::decay_t<decltype(StaticContainer)>::value_type;
        Array<InputType> OutputOversized {};
        OutputOversized.Size = std::size(StaticContainer);

        std::ranges::copy(StaticContainer, OutputOversized);
        return OutputOversized;
    }

    template<typename InputType>
    consteval auto GenerateSpan(auto Getter)
    {
        constexpr auto& StaticGetter = AsStatic<Getter>;
        constexpr auto NewArray = GenerateArray(StaticGetter);
        return std::span<InputType> { std::begin(NewArray), std::end(NewArray) };
    }

    consteval auto GenerateSpan(auto Getter)
    {
        constexpr auto& StaticGetter = AsStatic<Getter>;
        constexpr auto NewArray = GenerateArray(StaticGetter);

        using InputType = typename std::decay_t<decltype(NewArray)>::value_type;
        return std::span<InputType> { std::begin(NewArray), std::end(NewArray) };
    }

    constexpr decltype(auto) ToSpan(auto const& Input)
    {
        using InputType = typename std::decay_t<decltype(Input)>::value_type;
        return std::span<InputType> { std::data(Input), std::end(Input) };
    }

    consteval auto GenerateStringView(auto Getter)
    {
        constexpr auto& StaticGetter = AsStatic<Getter>;
        constexpr auto NewArray = GenerateArray(StaticGetter);

        using InputType = typename std::decay_t<decltype(NewArray)>::value_type;
        return std::basic_string_view<InputType> { std::begin(NewArray), std::end(NewArray) };
    }

    constexpr decltype(auto) ToView(auto const& Input)
    {
        using InputType = typename std::decay_t<decltype(Input)>::value_type;
        return std::basic_string_view<InputType> { std::begin(Input), std::end(Input) };
    }

    template<typename... Args>
    consteval decltype(auto) Invoke(Args &&...Arguments)
    {
        return std::invoke(std::forward<Args>(Arguments)...);
    }
} // namespace luvk