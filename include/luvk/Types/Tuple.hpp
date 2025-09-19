#pragma once

#include <tuple>
#include "luvk/Module.hpp"

namespace luvk
{
    template <typename... Types>
    using Tuple = std::tuple<Types...>;
} // namespace luvk
