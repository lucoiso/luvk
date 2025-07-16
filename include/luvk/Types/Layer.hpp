// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Modules/Module.hpp"
#include "luvk/Types/Pair.hpp"

#include <string>

namespace luvk
{
    /** Represents an instance or device layer */
    struct LUVKMODULE_API Layer
    {
        /** Whether the layer is enabled */
        bool Enabled{};

        /** Layer name */
        std::string Name{};

        /** Supported extensions for this layer */
        std::vector<Pair<std::string, bool>> Extensions;
    };
} // namespace luvk
