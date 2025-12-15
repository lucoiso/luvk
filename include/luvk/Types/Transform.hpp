// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Types/Array.hpp"

namespace luvk
{
    struct LUVKMODULE_API Transform
    {
        luvk::Array<float, 3> Position{0.F, 0.F, 0.F};
        luvk::Array<float, 3> Rotation{0.F, 0.F, 0.F};
        luvk::Array<float, 3> Scale{1.F, 1.F, 1.F};
    };
} // namespace luvk
