// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Types/Array.hpp"

#include <algorithm>
#include <array>
#include <execution>
#include <span>
#include <ranges>
#include <string>

namespace luvk
{
    // Functions inspired by Jason Turner's presentations and implementations that can be found on:
    // https://github.com/lefticus/tools/blob/main/include/lefticus/tools/static_views.hpp

    template<typename Value>
    concept IsIterable = requires(const Value &Input)
    {
        Input.begin();
        Input.end();
    };

    template<typename Callable>
    concept CreatesIterable = requires(const Callable &Input)
    {
        requires IsIterable<std::decay_t<decltype(Input())>>;
    };

    template <auto Data>
    inline LUVKMODULE_API constexpr auto const &AsStatic = Data;

    template<typename DataType>
    constexpr LUVKMODULE_API auto GenerateOversizedArray(DataType const &Data)
    {
        using InputType = typename DataType::value_type;
        luvk::Array<InputType> OutputOversized {};

        std::copy(std::begin(Data),
                  std::end(Data),
                  std::begin(OutputOversized));

        OutputOversized.Size = std::size(Data);

        return OutputOversized;
    }

    constexpr LUVKMODULE_API auto GenerateRightSized(CreatesIterable auto Getter)
    {
        constexpr auto GeneratedContainer = GenerateOversizedArray(Getter());
        using InputType = typename std::decay_t<decltype(GeneratedContainer)>::value_type;

        std::array<InputType, GeneratedContainer.Size> RightSizedArray {};

        std::copy(std::begin(GeneratedContainer),
                  std::end(GeneratedContainer),
                  std::begin(RightSizedArray));

        return RightSizedArray;
    }

    consteval LUVKMODULE_API auto ToSpan(CreatesIterable auto Getter)
    {
        constexpr auto& StaticData = AsStatic<GenerateRightSized(Getter)>;
        using InputType = typename std::decay_t<decltype(StaticData)>::value_type;
        return std::span<InputType const> { std::begin(StaticData), std::end(StaticData) };
    }

    consteval LUVKMODULE_API auto ToStringView(CreatesIterable auto Getter)
    {
        constexpr auto& StaticData = AsStatic<GenerateRightSized(Getter)>;
        using InputType = typename std::decay_t<decltype(StaticData)>::value_type;
        return std::basic_string_view<InputType> { std::begin(StaticData), std::end(StaticData) };
    }

    template <typename OutputType>
    consteval LUVKMODULE_API auto ToContainerSpan(CreatesIterable auto Getter)
    {
        constexpr std::size_t AllocationSize = 256U;

        constexpr auto AllocationResult = [&]
        {
            auto const InputData = Getter();
            using InputType = typename std::decay_t<decltype(InputData)>::value_type;
            using InputItemType = typename InputType::value_type;

            std::array<InputItemType, AllocationSize * AllocationSize> ConcatenatedDataArray {};
            std::array<std::size_t, AllocationSize> DataSizeArray {};

            auto LastIterator = std::begin(ConcatenatedDataArray);

            for (std::size_t Iterator = 0U; const auto &InputIt : InputData)
            {
                LastIterator = std::ranges::copy(InputIt, LastIterator).out;
                DataSizeArray.at(Iterator++) = std::size(InputIt);
            }

            std::size_t const DataSize = std::distance(std::begin(ConcatenatedDataArray), LastIterator);

            return std::tuple { std::size(InputData), DataSize, ConcatenatedDataArray, DataSizeArray};
        }();

        constexpr auto DataSize = std::get<1>(AllocationResult);

        static constexpr auto RightSizedData = [&]
        {
            auto& ConcatenatedDataArray = std::get<2>(AllocationResult);
            using InputType = typename std::decay_t<decltype(ConcatenatedDataArray)>::value_type;

            std::array<InputType, DataSize> Output;

            std::copy(std::begin(ConcatenatedDataArray),
                      std::begin(ConcatenatedDataArray) + static_cast<std::ptrdiff_t>(DataSize),
                      std::begin(Output));

            return Output;
        }();

        constexpr auto NumInputs = std::get<0>(AllocationResult);
        auto& DataSizeArray = std::get<3>(AllocationResult);

        std::array<OutputType, NumInputs> GeneratedArray {};
        std::size_t DataStart = 0U;

        using RightSizedFullType = decltype(RightSizedData);

        for (std::size_t Iterator = 0U; Iterator < std::size(GeneratedArray); ++Iterator)
        {
            std::size_t const& CurrentSize = DataSizeArray.at(Iterator);

            GeneratedArray.at(Iterator) = OutputType
            {
                static_cast<typename RightSizedFullType::const_iterator>(std::begin(RightSizedData) + static_cast<std::ptrdiff_t>(DataStart)),
                static_cast<typename RightSizedFullType::const_iterator>(std::begin(RightSizedData) + static_cast<std::ptrdiff_t>(DataStart + CurrentSize))
            };

            DataStart += CurrentSize;
        }

        return GeneratedArray;
    }
} // namespace luvk