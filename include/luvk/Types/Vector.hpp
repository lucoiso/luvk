#pragma once

#include <vector>
#include <cstddef>
#include "luvk/Module.hpp"

namespace luvk
{
    template <typename T, std::size_t Capacity = 256U, typename Allocator = std::allocator<T>>
    class LUVKMODULE_API Vector : public std::vector<T, Allocator>
    {
        using base = std::vector<T, Allocator>;

    public:
        Vector()
        {
            if constexpr (Capacity > 0U)
            {
                base::reserve(Capacity);
            }
        }

        explicit Vector(std::size_t const Count, T const& Value = T{}) : base(Count, Value) {}

        explicit Vector(std::initializer_list<T> const Init) : base(Init)
        {
            if constexpr (Capacity > 0U)
            {
                base::reserve(std::max(static_cast<std::size_t>(Capacity), Init.size()));
            }
        }

        template <typename... Args>
        explicit Vector(Args&&... ArgsIn) : base(std::forward<Args>(ArgsIn)...) {}
    };
} // namespace luvk
