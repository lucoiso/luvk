// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Libraries/Compile.hpp"
#include "luvk/Types/Pair.hpp"

#include <string>

namespace luvk
{
    struct LUVKMODULE_API Layer
    {
        bool Enabled{};
        std::string Name {};
        std::vector<Pair<std::string, bool>> Extensions;
    };
} // namespace luvk