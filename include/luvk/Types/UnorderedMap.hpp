#pragma once

#include "luvk/Types/Map.hpp"

namespace luvk
{
    template <typename Key, typename T, std::size_t Capacity = 64U>
    using UnorderedMap = Map<Key, T, Capacity>;
} // namespace luvk
