// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

namespace luvk
{
    template <typename FirstType, typename SecondType>
    struct LUVKMODULE_API Pair
    {
        FirstType First{};
        SecondType Second{};
    };
} // namespace luvk