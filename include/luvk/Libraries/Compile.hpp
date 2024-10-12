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
    struct LUVKMODULE_API OversizedArray
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
            return std::find(std::execution::par_unseq,
                             std::cbegin(Data),
                             std::cend(Data),
                             Item) != std::cend(Data);
        }

        [[nodiscard]] constexpr auto data() noexcept
        {
            return std::data(Data);
        }

        [[nodiscard]] constexpr auto begin() noexcept
        {
            return std::begin(Data);
        }

        [[nodiscard]] constexpr auto end() noexcept
        {
            return std::next(std::begin(Data),
                             static_cast<std::ptrdiff_t>(Size));
        }
    };

    constexpr auto GenerateOversized(auto Getter)
    {
        auto GeneratedContainer = Getter();
        constexpr auto& StaticContainer = AsStatic<GeneratedContainer>;

        using InputType = typename std::decay_t<decltype(StaticContainer)>::value_type;
        OversizedArray<InputType> OutputOversized {};
        OutputOversized.Size = std::size(StaticContainer);

        std::ranges::copy(StaticContainer, OutputOversized);
        return OutputOversized;
    }

    template<typename InputType>
    consteval auto GenerateSpan(auto Getter)
    {
        constexpr auto& StaticGetter = AsStatic<Getter>;
        constexpr auto GeneratedOversized = GenerateOversized(StaticGetter);
        return std::span<InputType> { std::begin(GeneratedOversized), std::end(GeneratedOversized) };
    }

    consteval auto GenerateSpan(auto Getter)
    {
        constexpr auto& StaticGetter = AsStatic<Getter>;
        constexpr auto GeneratedOversized = GenerateOversized(StaticGetter);

        using InputType = typename std::decay_t<decltype(GeneratedOversized)>::value_type;
        return std::span<InputType> { std::begin(GeneratedOversized), std::end(GeneratedOversized) };
    }

    template<typename InputType>
    constexpr std::span<InputType> ToSpan(OversizedArray<InputType> Input)
    {
        return { std::begin(Input), std::end(Input) };
    }

    consteval auto GenerateStringView(auto Getter)
    {
        constexpr auto& StaticGetter = AsStatic<Getter>;
        constexpr auto GeneratedOversized = GenerateOversized(StaticGetter);

        using InputType = typename std::decay_t<decltype(GeneratedOversized)>::value_type;
        return std::basic_string_view<InputType> { std::begin(GeneratedOversized), std::end(GeneratedOversized) };
    }

    template<typename... Args>
    consteval decltype(auto) Invoke(Args &&...Arguments)
    {
        return std::invoke(std::forward<Args>(Arguments)...);
    }
} // namespace luvk