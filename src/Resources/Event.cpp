// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Event.hpp"
#include <memory>

void luvk::EventNode::operator()()
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

std::shared_ptr<luvk::EventNode> luvk::EventNode::NewNode(std::function<void()>&& Function, const bool OneTime)
{
    return std::make_shared<EventNode>(std::move(Function), OneTime);
}

std::shared_ptr<luvk::EventNode> luvk::EventNode::Then(std::function<void()>&& Function, const bool OneTime)
{
    m_SubNodes.emplace_back(std::make_shared<EventNode>(std::move(Function), OneTime));
    return shared_from_this();
}

void luvk::EventGraph::AddNode(std::shared_ptr<EventNode>&& Node, const std::size_t Key)
{
    if (m_Nodes.contains(Key))
    {
        m_Nodes.at(Key).push_back(std::move(Node));
    }
    else
    {
        std::vector Nodes{std::move(Node)};
        m_Nodes.emplace(Key, std::move(Nodes));
    }
}

void luvk::EventGraph::Execute(const std::size_t Key)
{
    if (m_Nodes.contains(Key))
    {
        bool EmptyNode = false;

        {
            std::vector<std::shared_ptr<EventNode>>& Nodes = m_Nodes.at(Key);

            std::erase_if(Nodes,
                          [](const std::shared_ptr<EventNode>& NodeIt)
                          {
                              (*NodeIt)();
                              return NodeIt->IsOneTime();
                          });

            EmptyNode = std::empty(Nodes);
        }

        if (EmptyNode)
        {
            m_Nodes.erase(Key);
        }
    }
}
