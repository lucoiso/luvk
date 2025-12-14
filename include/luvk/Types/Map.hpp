// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <algorithm>
#include <stdexcept>
#include <utility>
#include "luvk/Types/Pair.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    template <typename Key, typename T, std::size_t Capacity = 64U>
    class Map
    {
        Vector<Pair<Key, T>, Capacity> m_Data{};

        [[nodiscard]] constexpr auto find_iterator(const Key& KeyValue) noexcept
        {
            return std::find_if(std::begin(m_Data),
                                std::end(m_Data),
                                [&](const auto& It)
                                {
                                    return It.First == KeyValue;
                                });
        }

        [[nodiscard]] constexpr auto find_iterator(const Key& KeyValue) const noexcept
        {
            return std::find_if(std::cbegin(m_Data),
                                std::cend(m_Data),
                                [&](const auto& It)
                                {
                                    return It.First == KeyValue;
                                });
        }

    public:
        using iterator       = typename decltype(m_Data)::iterator;
        using const_iterator = typename decltype(m_Data)::const_iterator;
        using value_type     = Pair<Key, T>;

        constexpr Map() = default;

        constexpr Map(std::initializer_list<value_type>&& Init)
        {
            for (const auto& Item : Init)
            {
                emplace(Item.First, Item.Second);
            }
        }

        [[nodiscard]] constexpr bool contains(const Key& KeyValue) const noexcept
        {
            return find_iterator(KeyValue) != std::cend(m_Data);
        }

        [[nodiscard]] constexpr iterator find(const Key& KeyValue) noexcept
        {
            return find_iterator(KeyValue);
        }

        [[nodiscard]] constexpr const_iterator find(const Key& KeyValue) const noexcept
        {
            return find_iterator(KeyValue);
        }

        [[nodiscard]] constexpr T& at(const Key& KeyValue)
        {
            auto It = find_iterator(KeyValue);
            if (It == std::end(m_Data))
            {
                throw std::out_of_range{"Key not found"};
            }
            return It->Second;
        }

        template <typename... Args>
        constexpr Pair<iterator, bool> emplace(const Key& KeyValue, Args&&... ArgsIn)
        {
            if (auto It = find_iterator(KeyValue);
                It != std::end(m_Data))
            {
                return {It, false};
            }
            m_Data.emplace_back(value_type{KeyValue, T{std::forward<Args>(ArgsIn)...}});
            return {std::end(m_Data) - 1, true};
        }

        constexpr void erase(const Key& KeyValue)
        {
            if (auto It = find_iterator(KeyValue);
                It != std::end(m_Data))
            {
                m_Data.erase(It);
            }
        }

        constexpr void clear() noexcept
        {
            m_Data.clear();
        }

        [[nodiscard]] constexpr iterator begin() noexcept
        {
            return std::begin(m_Data);
        }

        [[nodiscard]] constexpr iterator end() noexcept
        {
            return std::end(m_Data);
        }

        [[nodiscard]] constexpr const_iterator begin() const noexcept
        {
            return std::cbegin(m_Data);
        }

        [[nodiscard]] constexpr const_iterator end() const noexcept
        {
            return std::cend(m_Data);
        }

        [[nodiscard]] constexpr std::size_t size() const noexcept
        {
            return std::size(m_Data);
        }

        [[nodiscard]] constexpr bool empty() const noexcept
        {
            return std::empty(m_Data);
        }

        [[nodiscard]] constexpr T& operator[](const Key& KeyValue)
        {
            auto [It, Inserted] = emplace(KeyValue, T{});
            return It->Second;
        }
    };
} // namespace luvk
