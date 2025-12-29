/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <string_view>
#include <unordered_map>
#include <vector>

namespace luvk
{
    /** Type alias for a map of Vulkan Layer Name -> List of Extension Names. */
    using ExtensionMap = std::unordered_map<std::string_view, std::vector<std::string_view>>;

    /**
     * Interface for modules that require Vulkan Instance or Device extensions.
     */
    class LUVK_API IExtensionsModule
    {
    public:
        /** Virtual destructor. */
        virtual ~IExtensionsModule() = default;

        /**
         * Get required instance extensions and layers.
         * @return Map of Layer Name -> List of Extensions.
         */
        [[nodiscard]] virtual ExtensionMap GetInstanceExtensions() const noexcept
        {
            return {};
        }

        /**
         * Get required device extensions and layers.
         * @return Map of Layer Name -> List of Extensions.
         */
        [[nodiscard]] virtual ExtensionMap GetDeviceExtensions() const noexcept
        {
            return {};
        }
    };
}
