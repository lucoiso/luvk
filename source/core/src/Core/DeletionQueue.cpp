// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Core/DeletionQueue.hpp"
#include <cstdio>

luvk::DeletionQueue::~DeletionQueue()
{
    std::size_t Leaked = 0U;

    {
        const std::scoped_lock Lock(m_Mutex);
        Leaked += std::size(m_Incoming);
        m_Incoming.clear();
    }
    Leaked += std::size(m_Pending);
    m_Pending.clear();

    if (Leaked > 0U)
    {
        std::fprintf(stderr, "[luvk] DeletionQueue destroyed with %zu pending objects.\n", Leaked);
    }
}

void luvk::DeletionQueue::Push(RetiredObject&& Object, const TimelinePoint& Point)
{
    const std::scoped_lock Lock(m_Mutex);
    m_Incoming.push_back({.Object = std::move(Object), .Point = Point});
    m_Size.fetch_add(1U, std::memory_order_relaxed);
}
