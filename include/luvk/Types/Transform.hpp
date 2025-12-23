// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>

namespace luvk
{
    struct LUVK_API Transform
    {
        std::array<float, 3> Position{0.F, 0.F, 0.F};
        std::array<float, 3> Rotation{0.F, 0.F, 0.F};
        std::array<float, 3> Scale{1.F, 1.F, 1.F};
    };
} // namespace luvk
