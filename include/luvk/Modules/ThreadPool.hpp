// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class LUVKMODULE_API ThreadPool : public IRenderModule
    {
        Vector<std::thread> m_Threads{};
        std::queue<std::function<void()>> m_Tasks{};
        std::mutex m_Mutex{};
        std::condition_variable m_Condition{};
        std::size_t m_Active{0};
        bool m_Stop{false};

    public:
        constexpr ThreadPool() = default;
        ~ThreadPool() override;

        void Start(std::uint32_t ThreadCount);
        void Submit(std::function<void()> Task);
        void WaitIdle();

    private:
        void Worker();
        void InitializeDependencies(const std::shared_ptr<IRenderModule>&) override;
        void ClearResources() override;
    };
} // namespace luvk
