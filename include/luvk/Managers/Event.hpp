/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace luvk
{
    class EventGraph;

    /** Handle to a subscribed event node, allows unbinding on destruction */
    class LUVK_API EventHandle
    {
        friend class EventGraph;

        /** Weak pointer to the owning EventGraph. */
        std::weak_ptr<EventGraph> m_Graph;

        /** The key in the EventGraph map. */
        std::size_t m_Key{0};

        /** Unique ID of the subscribed node within the key's list. */
        std::size_t m_Id{0};

        /** Private constructor used by EventGraph::AddNode. */
        EventHandle(std::weak_ptr<EventGraph> Graph, std::size_t Key, std::size_t Id);

    public:
        /** Default constructor */
        EventHandle() = default;

        /** Destructor unbinds event automatically */
        ~EventHandle();

        /** Move constructor */
        EventHandle(EventHandle&& Other) noexcept;

        /** Move assignment operator */
        EventHandle& operator=(EventHandle&& Other) noexcept;

        /** Manually unbind this event */
        void Unbind();
    };

    /** Node in an event chain that can execute a callback and chain to other nodes */
    class LUVK_API EventNode : public std::enable_shared_from_this<EventNode>
    {
    protected:
        /** Flag indicating if the node should execute only once. */
        bool m_OneTime{false};

        /** The function/callback to execute. */
        std::function<void()> m_Binding{};

        /** Nodes that will execute after this one. */
        std::vector<std::shared_ptr<EventNode>> m_SubNodes{};

    public:
        /** Default constructor */
        constexpr EventNode() = default;

        /** Destructor */
        ~EventNode() = default;

        /** Constructor with function and one-time execution flag */
        explicit EventNode(std::function<void()>&& Function, const bool OneTime = false)
            : m_OneTime(OneTime),
              m_Binding(std::move(Function)) {}

        /** Execute this node and its sub-nodes */
        void operator()();

        /** Check if node executes only once */
        [[nodiscard]] constexpr bool IsOneTime() const noexcept
        {
            return m_OneTime;
        }

        /** Create a new event node */
        [[nodiscard]] static std::shared_ptr<EventNode> NewNode(std::function<void()>&& Function, bool OneTime = false);

        /** Chain another node to execute after this one */
        [[nodiscard]] std::shared_ptr<EventNode> Then(std::function<void()>&& Function, bool OneTime = false);
    };

    /** Graph of connected event nodes, indexed by key */
    class LUVK_API EventGraph : public std::enable_shared_from_this<EventGraph>
    {
        /** Internal structure to hold the node and its unique ID. */
        struct NodeData
        {
            std::size_t Id;
            std::shared_ptr<EventNode> Node;
        };

        /** Map of event key (hash/size_t) to a list of event nodes. */
        std::unordered_map<std::size_t, std::vector<NodeData>> m_Nodes{};

        /** Counter for generating unique node IDs. */
        std::size_t m_NextNodeId{0};

    public:
        /** Default constructor */
        constexpr EventGraph() = default;

        /** Destructor */
        ~EventGraph() = default;

        /** Add a node to the graph under a specific key */
        [[nodiscard]] EventHandle AddNode(std::shared_ptr<EventNode>&& Node, std::size_t Key);

        /** Remove a node from the graph */
        void RemoveNode(std::size_t Key, std::size_t Id);

        /** Add node with enum key */
        template <typename KeyType> requires std::is_enum_v<KeyType>
        constexpr EventHandle AddNode(std::shared_ptr<EventNode>&& Node, KeyType Key)
        {
            return AddNode(std::move(Node), static_cast<std::size_t>(Key));
        }

        /** Execute all nodes under a given key */
        void Execute(std::size_t Key);

        /** Execute all nodes under an enum key */
        template <typename KeyType> requires std::is_enum_v<KeyType>
        constexpr void Execute(KeyType Key)
        {
            Execute(static_cast<std::size_t>(Key));
        }
    };
}
