/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace luvk
{
    class EventGraph;

    class LUVK_API EventHandle
    {
        friend class EventGraph;
        std::weak_ptr<EventGraph> m_Graph;
        std::size_t               m_Key{0};
        std::size_t               m_Id{0};

        EventHandle(std::weak_ptr<EventGraph> Graph, std::size_t Key, std::size_t Id);

    public:
        EventHandle() = default;
        ~EventHandle();

        EventHandle(EventHandle&& Other) noexcept;
        EventHandle& operator=(EventHandle&& Other) noexcept;

        void Unbind();
    };

    class LUVK_API EventNode : public std::enable_shared_from_this<EventNode>
    {
    protected:
        bool                                    m_OneTime{false};
        std::function<void()>                   m_Binding{};
        std::vector<std::shared_ptr<EventNode>> m_SubNodes{};

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

    class LUVK_API EventGraph : public std::enable_shared_from_this<EventGraph>
    {
        struct NodeData
        {
            std::size_t                Id;
            std::shared_ptr<EventNode> Node;
        };

        std::unordered_map<std::size_t, std::vector<NodeData>> m_Nodes{};
        std::size_t                                            m_NextNodeId{0};

    public:
        constexpr EventGraph() = default;
        ~EventGraph()          = default;

        [[nodiscard]] EventHandle AddNode(std::shared_ptr<EventNode>&& Node, std::size_t Key);
        void                      RemoveNode(std::size_t Key, std::size_t Id);

        template <typename KeyType> requires std::is_enum_v<KeyType>
        constexpr EventHandle AddNode(std::shared_ptr<EventNode>&& Node, KeyType Key)
        {
            return AddNode(std::move(Node), static_cast<std::size_t>(Key));
        }

        void Execute(std::size_t Key);

        template <typename KeyType> requires std::is_enum_v<KeyType>
        constexpr void Execute(KeyType Key)
        {
            Execute(static_cast<std::size_t>(Key));
        }
    };
}
