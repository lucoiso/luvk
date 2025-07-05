// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <functional>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace luvk
{
    /** Thread pool used for multithreaded rendering */
    class LUVKMODULE_API ThreadPool : public IRenderModule
    {
        /** Worker threads */
        std::vector<std::thread> m_Threads{};

        /** Pending tasks */
        std::queue<std::function<void()>> m_Tasks{};

        /** Mutex protecting the task queue */
        std::mutex m_Mutex{};

        /** Condition variable for worker wakeups */
        std::condition_variable m_Condition{};

        /** Number of tasks currently running */
        std::size_t m_Active{0};

        /** True when the pool is shutting down */
        bool m_Stop{false};

    public:
        constexpr ThreadPool() = default;

        //~ Begin of IRenderModule interface
        ~ThreadPool() override;

        /** Start the thread pool */
        void Start(std::uint32_t ThreadCount);

        /** Submit a task to the pool */
        void Submit(std::function<void()> Task);

        /** Wait until all tasks are processed */
        void WaitIdle();

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const& DeviceModule) const noexcept override
        {
            return nullptr;
        }

        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

    private: /** Worker thread entry */
        void Worker();

        /** Setup dependencies after renderer initialization */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Destroy threads and clear remaining tasks */
        void ClearResources() override;
        //~ End of IRenderModule interface
    };
} // namespace luvk
