#pragma once

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include "luvk/Module.hpp"
#include "luvk/Types/Pair.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    template <typename Key, typename T, std::size_t Capacity = 64U>
    class LUVKMODULE_API Map
    {
        luvk::Vector<luvk::Pair<Key, T>, Capacity> m_Data{};

        [[nodiscard]] constexpr auto find_iterator(Key const& KeyValue) noexcept
        {
            return std::find_if(std::begin(m_Data), std::end(m_Data), [&](auto const& It) { return It.First == KeyValue; });
        }

        [[nodiscard]] constexpr auto find_iterator(Key const& KeyValue) const noexcept
        {
            return std::find_if(std::begin(m_Data), std::end(m_Data), [&](auto const& It) { return It.First == KeyValue; });
        }

    public:
        using iterator = typename decltype(m_Data)::iterator;
        using const_iterator = typename decltype(m_Data)::const_iterator;
        using value_type = luvk::Pair<Key, T>;

        constexpr Map() = default;
        constexpr explicit Map(std::initializer_list<value_type> Init) { for (auto const& Item : Init) emplace(Item.First, Item.Second); }

        [[nodiscard]] constexpr bool contains(Key const& KeyValue) const noexcept
        {
            return find_iterator(KeyValue) != std::end(m_Data);
        }

        [[nodiscard]] constexpr iterator find(Key const& KeyValue) noexcept
        {
            return find_iterator(KeyValue);
        }

        [[nodiscard]] constexpr const_iterator find(Key const& KeyValue) const noexcept
        {
            return find_iterator(KeyValue);
        }

        [[nodiscard]] constexpr T& at(Key const& KeyValue)
        {
            auto It = find_iterator(KeyValue);
            if (It == std::end(m_Data))
            {
                throw std::out_of_range{"Key not found"};
            }
            return It->Second;
        }

        [[nodiscard]] constexpr T const& at(Key const& KeyValue) const
        {
            auto It = find_iterator(KeyValue);
            if (It == std::end(m_Data))
            {
                throw std::out_of_range{"Key not found"};
            }
            return It->Second;
        }

        template <typename... Args>
        constexpr luvk::Pair<iterator, bool> emplace(Key const& KeyValue, Args&&... ArgsIn)
        {
            if (auto It = find_iterator(KeyValue); It != std::end(m_Data))
            {
                return {It, false};
            }
            m_Data.emplace_back(value_type{KeyValue, T{std::forward<Args>(ArgsIn)...}});
            return {std::end(m_Data) - 1, true};
        }

        constexpr iterator erase(const_iterator const Position)
        {
            return m_Data.erase(Position);
        }

        constexpr void erase(Key const& KeyValue)
        {
            if (auto It = find_iterator(KeyValue); It != std::end(m_Data))
            {
                m_Data.erase(It);
            }
        }

        constexpr void clear() noexcept
        {
            m_Data.clear();
        }

        constexpr void reserve(std::size_t) const noexcept {}

        [[nodiscard]] constexpr iterator begin() noexcept { return std::begin(m_Data); }
        [[nodiscard]] constexpr iterator end() noexcept { return std::end(m_Data); }
        [[nodiscard]] constexpr const_iterator begin() const noexcept { return std::begin(m_Data); }
        [[nodiscard]] constexpr const_iterator end() const noexcept { return std::end(m_Data); }
        [[nodiscard]] constexpr std::size_t size() const noexcept { return m_Data.size(); }

        [[nodiscard]] constexpr T& operator[](Key const& KeyValue)
        {
            auto [It, Inserted] = emplace(KeyValue, T{});
            return It->Second;
        }
    };
} // namespace luvk
