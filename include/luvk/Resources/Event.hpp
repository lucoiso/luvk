// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <functional>
#include "luvk/Types/Map.hpp"
#include "luvk/Types/Vector.hpp"
#include "luvk/Module.hpp"

namespace luvk
{
    class LUVKMODULE_API EventNode
    {
        bool m_OneTime{false};
        std::function<void()> m_Binding{};
        luvk::Vector<EventNode> m_SubNodes{};

    public:
        constexpr EventNode() = default;
        ~EventNode() = default;

        explicit EventNode(std::function<void()>&& Function, bool const OneTime = false)
            : m_OneTime(OneTime),
              m_Binding(std::move(Function)) {}

        void operator()();

        [[nodiscard]] constexpr bool IsOneTime() const
        {
            return m_OneTime;
        }

        [[nodiscard]] static EventNode NewNode(std::function<void()>&& Function, bool OneTime = false);
        [[nodiscard]] EventNode& Then(std::function<void()>&& Function, bool OneTime = false);
    };

    class LUVKMODULE_API EventGraph
    {
        luvk::Map<std::size_t, luvk::Vector<EventNode>> m_Nodes{};

    public:
        constexpr EventGraph() = default;
        ~EventGraph() = default;

        void AddNode(EventNode&& Node, std::size_t Key);

        template <typename KeyType> requires std::is_enum_v<KeyType>
        constexpr void AddNode(EventNode&& Node, KeyType Key)
        {
            AddNode(std::move(Node), static_cast<std::size_t>(Key));
        }

        void Execute(std::size_t Key);

        template <typename KeyType> requires std::is_enum_v<KeyType>
        constexpr void Execute(KeyType Key)
        {
            Execute(static_cast<std::size_t>(Key));
        }
    };
} // namespace luvk
