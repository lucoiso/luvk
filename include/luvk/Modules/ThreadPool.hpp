// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class LUVK_API ThreadPool : public IRenderModule
    {
    protected:
        std::vector<std::thread>          m_Threads{};
        std::queue<std::function<void()>> m_Tasks{};
        std::mutex                        m_Mutex{};
        std::condition_variable           m_Condition{};
        std::size_t                       m_Active{0};
        bool                              m_Stop{false};

    public:
        constexpr ThreadPool() = default;

        ~ThreadPool() override
        {
            ThreadPool::ClearResources();
        }

        void Start(std::uint32_t ThreadCount);
        void Submit(std::function<void()> Task);
        void WaitIdle();

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return m_Stop;
        }

        [[nodiscard]] std::uint32_t GetActiveThreadCount() const noexcept
        {
            return m_Active;
        }

        [[nodiscard]] std::uint32_t GetThreadCount() const noexcept
        {
            return std::size(m_Threads);
        }

    protected:
        void ClearResources() override;

    private:
        void Worker();
    };
} // namespace luvk
