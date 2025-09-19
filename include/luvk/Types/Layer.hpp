// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string>
#include "luvk/Module.hpp"
#include "luvk/Types/Pair.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    struct LUVKMODULE_API Layer
    {
        bool Enabled{};
        std::string Name{};
        Vector<Pair<std::string, bool>> Extensions;
    };
} // namespace luvk
