// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include <cstdint>
#include <vector>
#include <map>
#include <functional>

namespace luvk
{
    /** Object that will manage events */
    class LUVKMODULE_API EventNode
    {
        /** True if the node should execute only once */
        bool m_OneTime{false};

        /** Function bound to this node */
        std::function<void()> m_Binding{};

        /** List of child nodes to be executed after this */
        std::vector<EventNode> m_SubNodes{};

    public:
        constexpr EventNode() = default;
        ~EventNode() = default;

        explicit EventNode(std::function<void()>&& Function, bool const OneTime = false)
            : m_OneTime(OneTime),
              m_Binding(std::move(Function)) {}

        /** Binding caller */
        void operator()();

        [[nodiscard]] constexpr bool IsOneTime() const
        {
            return m_OneTime;
        }

        /** Create new root node */
        [[nodiscard]] static EventNode NewNode(std::function<void()>&& Function, bool OneTime = false);

        /** Add new sub node */
        [[nodiscard]] EventNode& Then(std::function<void()>&& Function, bool OneTime = false);
    };

    /** Object that will manage event graphs */
    class LUVKMODULE_API EventGraph
    {
        /** Nodes organized by event key */
        std::map<std::size_t, std::vector<EventNode>> m_Nodes{};

    public:
        constexpr EventGraph() = default;
        ~EventGraph() = default;

        /** Add new node to the graph */
        void AddNode(EventNode&& Node, std::size_t Key);

        /** Add new node to the graph */
        template <typename KeyType> requires std::is_enum_v<KeyType>
        constexpr void AddNode(EventNode&& Node, KeyType Key)
        {
            AddNode(std::move(Node), static_cast<std::size_t>(Key));
        }

        /** Execute the events registered under the specified key */
        void Execute(std::size_t Key);

        /** Execute the events registered under the specified key */
        template <typename KeyType> requires std::is_enum_v<KeyType>
        constexpr void Execute(KeyType Key)
        {
            Execute(static_cast<std::size_t>(Key));
        }
    };
} // namespace luvk
