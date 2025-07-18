// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Event.hpp"

void luvk::EventNode::operator()()
{
    if (m_Binding)
    {
        m_Binding();
    }

    std::erase_if(m_SubNodes,
                  [](EventNode& NodeIt)
                  {
                      NodeIt();
                      return NodeIt.IsOneTime();
                  });
}

luvk::EventNode luvk::EventNode::NewNode(std::function<void()>&& Function, bool const OneTime)
{
    return EventNode{std::move(Function), OneTime};
}

luvk::EventNode& luvk::EventNode::Then(std::function<void()>&& Function, bool const OneTime)
{
    m_SubNodes.emplace_back(std::move(Function), OneTime);
    return *this;
}

void luvk::EventGraph::AddNode(EventNode&& Node, std::size_t const Key)
{
    if (m_Nodes.contains(Key))
    {
        m_Nodes.at(Key).push_back(std::move(Node));
    }
    else
    {
        m_Nodes.emplace(Key, std::vector{std::move(Node)});
    }
}

void luvk::EventGraph::Execute(std::size_t const Key)
{
    if (m_Nodes.contains(Key))
    {
        bool EmptyNode = false;

        {
            std::vector<EventNode>& Nodes = m_Nodes.at(Key);

            std::erase_if(Nodes,
                          [](EventNode& NodeIt)
                          {
                              NodeIt();
                              return NodeIt.IsOneTime();
                          });

            EmptyNode = std::empty(Nodes);
        }

        if (EmptyNode)
        {
            m_Nodes.erase(Key);
        }
    }
}
