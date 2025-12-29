/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Modules/ThreadPool.hpp"

using namespace luvk;

void ThreadPool::OnInitialize(IServiceLocator* ServiceLocator)
{
    const std::uint32_t Count = std::thread::hardware_concurrency();
    for (std::uint32_t It = 0; It < Count; ++It)
    {
        m_Threads.emplace_back(&ThreadPool::Worker, this);
    }

    IModule::OnInitialize(ServiceLocator);
}

void ThreadPool::OnShutdown()
{
    {
        std::lock_guard Lock(m_Mutex);
        m_Stop = true;
    }

    m_Condition.notify_all();

    for (auto& Worker : m_Threads)
    {
        if (Worker.joinable()) Worker.join();
    }
    m_Threads.clear();

    IModule::OnShutdown();
}

void ThreadPool::Enqueue(std::function<void()> Task)
{
    {
        std::lock_guard Lock(m_Mutex);
        m_Tasks.push(std::move(Task));
    }
    m_Condition.notify_one();
}

void ThreadPool::WaitIdle()
{
    std::unique_lock Lock(m_Mutex);

    m_Condition.wait(Lock,
                     [this]
                     {
                         return std::empty(m_Tasks);
                     });
}

void ThreadPool::Worker()
{
    while (true)
    {
        std::function<void()> Task;
        {
            std::unique_lock Lock(m_Mutex);
            m_Condition.wait(Lock,
                             [this]
                             {
                                 return m_Stop || !m_Tasks.empty();
                             });

            if (m_Stop && m_Tasks.empty()) return;

            Task = std::move(m_Tasks.front());
            m_Tasks.pop();
        }
        Task();
    }
}
