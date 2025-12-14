// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <initializer_list>
#include <string_view>
#include "luvk/Module.hpp"
#include "luvk/Types/UnorderedMap.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    using ExtensionsMap = UnorderedMap<std::string_view, Vector<std::string_view>>;

    template <typename KeyType, typename ValueType>
    ExtensionsMap ToExtensionMap(KeyType&& Key, std::initializer_list<ValueType>&& Values)
    {
        Vector<std::string_view> NewEntries;
        NewEntries.reserve(std::size(Values));

        for (const auto& ValueIt : Values)
        {
            NewEntries.push_back(std::string_view{ValueIt});
        }

        return ExtensionsMap{{std::string_view{Key}, std::move(NewEntries)}};
    }

    class LUVKMODULE_API IExtensionsModule
    {
    public:
        virtual ~IExtensionsModule() = default;

        [[nodiscard]] virtual ExtensionsMap GetInstanceExtensions() const
        {
            return {};
        }

        [[nodiscard]] virtual ExtensionsMap GetDeviceExtensions() const
        {
            return {};
        }
    };
} // namespace luvk
