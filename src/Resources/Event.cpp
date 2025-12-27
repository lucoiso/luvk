/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Event.hpp"
#include <algorithm>

using namespace luvk;

EventHandle::EventHandle(std::weak_ptr<EventGraph> Graph, const std::size_t Key, const std::size_t Id)
    : m_Graph(std::move(Graph)),
      m_Key(Key),
      m_Id(Id) {}

EventHandle::~EventHandle()
{
    Unbind();
}

EventHandle::EventHandle(EventHandle&& Other) noexcept
    : m_Graph(std::move(Other.m_Graph)),
      m_Key(Other.m_Key),
      m_Id(Other.m_Id)
{
    Other.m_Id = 0;
}

EventHandle& EventHandle::operator=(EventHandle&& Other) noexcept
{
    if (this != &Other)
    {
        Unbind();
        m_Graph    = std::move(Other.m_Graph);
        m_Key      = Other.m_Key;
        m_Id       = Other.m_Id;
        Other.m_Id = 0;
    }
    return *this;
}

void EventHandle::Unbind()
{
    if (m_Id != 0)
    {
        if (const auto Ptr = m_Graph.lock())
        {
            Ptr->RemoveNode(m_Key, m_Id);
        }
        m_Id = 0;
    }
}

void EventNode::operator()()
{
    if (m_Binding)
    {
        m_Binding();
    }

    std::erase_if(m_SubNodes,
                  [](const std::shared_ptr<EventNode>& NodeIt)
                  {
                      (*NodeIt)();
                      return NodeIt->IsOneTime();
                  });
}

std::shared_ptr<EventNode> EventNode::NewNode(std::function<void()>&& Function, const bool OneTime)
{
    return std::make_shared<EventNode>(std::move(Function), OneTime);
}

std::shared_ptr<EventNode> EventNode::Then(std::function<void()>&& Function, const bool OneTime)
{
    m_SubNodes.emplace_back(std::make_shared<EventNode>(std::move(Function), OneTime));
    return shared_from_this();
}

EventHandle EventGraph::AddNode(std::shared_ptr<EventNode>&& Node, const std::size_t Key)
{
    if (!m_Nodes.contains(Key))
    {
        m_Nodes.emplace(Key, std::vector<NodeData>{});
    }

    const std::size_t Id = ++m_NextNodeId;
    m_Nodes.at(Key).push_back(NodeData{Id,
                                       std::move(Node)});

    return EventHandle(shared_from_this(), Key, Id);
}

void EventGraph::RemoveNode(const std::size_t Key, const std::size_t Id)
{
    if (const auto It = m_Nodes.find(Key);
        It != std::end(m_Nodes))
    {
        std::erase_if(It->second,
                      [Id](const NodeData& Data)
                      {
                          return Data.Id == Id;
                      });
    }
}

void EventGraph::Execute(const std::size_t Key)
{
    if (const auto It = m_Nodes.find(Key);
        It != std::end(m_Nodes))
    {
        std::erase_if(It->second,
                      [](const NodeData& Data)
                      {
                          (*Data.Node)();
                          return Data.Node->IsOneTime();
                      });

        if (std::empty(It->second))
        {
            m_Nodes.erase(It);
        }
    }
}
