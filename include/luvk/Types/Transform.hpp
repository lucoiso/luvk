// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** Transform.hpp definitions */

#include "luvk/Module.hpp"
#include <array>

namespace luvk
{
    /** Basic transform with position, rotation and scale */
    struct LUVKMODULE_API Transform
    {
        /** Position vector */
        std::array<float, 3> Position{0.F, 0.F, 0.F};

        /** Rotation expressed as Euler angles */
        std::array<float, 3> Rotation{0.F, 0.F, 0.F};

        /** Object scale */
        std::array<float, 3> Scale{1.F, 1.F, 1.F};
    };
} // namespace luvk
