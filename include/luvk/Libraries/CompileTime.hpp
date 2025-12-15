// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <algorithm>
#include <span>
#include "luvk/Module.hpp"
#include "luvk/Types/Array.hpp"
#include "luvk/Types/Array.hpp"

namespace luvk
{
    template <typename Value>
    concept IsIterable = requires(const Value& Input)
    {
        std::begin(Input);
        std::end(Input);
    };

    template <typename Callable>
    concept CreatesIterable = requires(const Callable& Input)
    {
        requires IsIterable<std::decay_t<decltype(Input())>>;
    };

    template <auto Data>
    inline LUVKMODULE_API constexpr const auto& AsStatic = Data;

    template <typename DataType>
    constexpr LUVKMODULE_API auto GenerateOversizedArray(const DataType& Data)
    {
        using InputType = DataType::value_type;
        Array<InputType> OutputOversized{};

        std::copy(std::begin(Data), std::end(Data), std::begin(OutputOversized));
        OutputOversized.Size = std::size(Data);

        return OutputOversized;
    }

    constexpr LUVKMODULE_API auto GenerateRightSized(CreatesIterable auto Getter)
    {
        constexpr auto GeneratedContainer = GenerateOversizedArray(Getter());
        using InputType                   = std::decay_t<decltype(GeneratedContainer)>::value_type;

        luvk::Array<InputType, GeneratedContainer.Size> RightSizedArray{};
        std::copy(std::begin(GeneratedContainer), std::end(GeneratedContainer), std::begin(RightSizedArray));

        return RightSizedArray;
    }

    consteval LUVKMODULE_API auto ToSpan(CreatesIterable auto Getter)
    {
        constexpr auto& StaticData = AsStatic<GenerateRightSized(Getter)>;
        using InputType            = std::decay_t<decltype(StaticData)>::value_type;
        return std::span<const InputType>{std::begin(StaticData), std::end(StaticData)};
    }
} // namespace luvk
