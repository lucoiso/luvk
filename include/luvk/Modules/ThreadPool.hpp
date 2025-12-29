/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "luvk/Interfaces/IModule.hpp"

namespace luvk
{
    /**
     * Module implementing a simple thread pool for asynchronous task execution.
     */
    class LUVK_API ThreadPool : public IModule
    {
        /** Collection of worker threads. */
        std::vector<std::thread> m_Threads{};

        /** Queue of tasks to be executed by worker threads. */
        std::queue<std::function<void()>> m_Tasks{};

        /** Mutex to protect access to the task queue. */
        std::mutex m_Mutex{};

        /** Condition variable to notify worker threads of new tasks. */
        std::condition_variable m_Condition{};

        /** Flag to signal the worker threads to stop. */
        bool m_Stop{false};

    public:
        /** Default destructor. */
        ~ThreadPool() override = default;

        /** Called upon module initialization (starts worker threads). */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /** Called upon module shutdown (stops and joins worker threads). */
        void OnShutdown() override;

        /**
         * Adds a task to the queue for asynchronous execution.
         * @param Task The function to execute.
         */
        void Enqueue(std::function<void()> Task);
        /**
         * Blocks the calling thread until all tasks in the queue are completed.
         */
        void WaitIdle();

    private:
        /** The main function executed by each worker thread. */
        void Worker();
    };
}
