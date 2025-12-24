// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

namespace luvk
{
    using ExtensionMap = std::unordered_map<std::string_view, std::vector<std::string_view>>;

    class LUVK_API IExtensionsModule
    {
    public:
        virtual ~IExtensionsModule() = default;

        [[nodiscard]] virtual ExtensionMap GetInstanceExtensions() const noexcept
        {
            return {};
        }

        [[nodiscard]] virtual ExtensionMap GetDeviceExtensions() const noexcept
        {
            return {};
        }
    };
} // namespace luvk
