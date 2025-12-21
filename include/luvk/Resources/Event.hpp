// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include "luvk/Module.hpp"
#include "luvk/Types/Map.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class LUVKMODULE_API EventNode : public std::enable_shared_from_this<EventNode>
    {
    protected:
        bool                               m_OneTime{false};
        std::function<void()>              m_Binding{};
        Vector<std::shared_ptr<EventNode>> m_SubNodes{};

    public:
        constexpr EventNode() = default;
        ~EventNode()          = default;

        explicit EventNode(std::function<void()>&& Function, const bool OneTime = false)
            : m_OneTime(OneTime),
              m_Binding(std::move(Function)) {}

        void operator()();

        [[nodiscard]] constexpr bool IsOneTime() const noexcept
        {
            return m_OneTime;
        }

        [[nodiscard]] static std::shared_ptr<EventNode> NewNode(std::function<void()>&& Function, bool OneTime = false);
        [[nodiscard]] std::shared_ptr<EventNode>        Then(std::function<void()>&& Function, bool OneTime = false);
    };

    class LUVKMODULE_API EventGraph
    {
        Map<std::size_t, Vector<std::shared_ptr<EventNode>>> m_Nodes{};

    public:
        constexpr EventGraph() = default;
        ~EventGraph()          = default;

        void AddNode(std::shared_ptr<EventNode>&& Node, std::size_t Key);

        template <typename KeyType>
            requires std::is_enum_v<KeyType>
        constexpr void AddNode(std::shared_ptr<EventNode>&& Node, KeyType Key)
        {
            AddNode(std::move(Node), static_cast<std::size_t>(Key));
        }

        void Execute(std::size_t Key);

        template <typename KeyType>
            requires std::is_enum_v<KeyType>
        constexpr void Execute(KeyType Key)
        {
            Execute(static_cast<std::size_t>(Key));
        }
    };
} // namespace luvk
