// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <concepts>
#include <vector>

namespace luvk
{
    template <typename T, std::size_t Capacity = 256U, typename Allocator = std::allocator<T>>
    class Vector : public std::vector<T, Allocator>
    {
        using base = std::vector<T, Allocator>;

    public:
        Vector() : base()
        {
            if constexpr (Capacity > 0U)
            {
                base::reserve(Capacity);
            }
        }

        Vector(std::initializer_list<T>&& Init) : base(Init)
        {
            if constexpr (Capacity > 0U)
            {
                base::reserve(std::max(Capacity, std::size(Init)));
            }
        }

        template <typename... Args>
            requires std::constructible_from<base, Args...>
        explicit Vector(Args&&... ArgsIn) : base(std::forward<Args>(ArgsIn)...) {}

        Vector(const Vector&)            = default;
        Vector(Vector&&)                 = default;
        Vector& operator=(const Vector&) = default;
        Vector& operator=(Vector&&)      = default;
        using base::operator=;
    };
} // namespace luvk
