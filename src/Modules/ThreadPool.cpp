// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Modules/ThreadPool.hpp"
#include <iterator>

void luvk::ThreadPool::Start(const std::uint32_t ThreadCount)
{
    m_Stop = false;

    for (std::uint32_t ThreadIndex = 0; ThreadIndex < ThreadCount; ++ThreadIndex)
    {
        m_Threads.emplace_back(&ThreadPool::Worker, this);
    }
}

void luvk::ThreadPool::Submit(std::function<void()> Task)
{
    {
        std::lock_guard Lock(m_Mutex);
        m_Tasks.emplace(std::move(Task));
    }

    m_Condition.notify_one();
}

void luvk::ThreadPool::WaitIdle()
{
    std::unique_lock Lock(m_Mutex);

    m_Condition.wait(Lock,
                     [this]
                     {
                         return std::empty(m_Tasks) && m_Active == 0;
                     });
}

void luvk::ThreadPool::ClearResources()
{
    {
        std::lock_guard Lock(m_Mutex);
        m_Stop = true;
    }

    m_Condition.notify_all();

    for (auto& Thread : m_Threads)
    {
        if (Thread.joinable())
        {
            Thread.join();
        }
    }

    m_Threads.clear();
}

void luvk::ThreadPool::Worker()
{
    while (true)
    {
        std::function<void()> Task;
        {
            std::unique_lock Lock(m_Mutex);
            m_Condition.wait(Lock,
                             [this]
                             {
                                 return m_Stop || !std::empty(m_Tasks);
                             });

            if (m_Stop && std::empty(m_Tasks))
            {
                return;
            }

            Task = std::move(m_Tasks.front());
            m_Tasks.pop();

            ++m_Active;
        }

        Task();

        {
            std::lock_guard Lock(m_Mutex);
            --m_Active;
        }

        m_Condition.notify_all();
    }
}
