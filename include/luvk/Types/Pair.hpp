// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

namespace luvk
{
    /** Simple pair structure */
    template <typename FirstType, typename SecondType>
    struct LUVKMODULE_API Pair
    {
        /** First value */
        FirstType First{};

        /** Second value */
        SecondType Second{};
    };
} // namespace luvk
