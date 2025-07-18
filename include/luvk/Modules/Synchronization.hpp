// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <iterator>
#include <memory>
#include "luvk/Types/Vector.hpp"
#include <volk/volk.h>
#include "luvk/Resources/CommandBufferPool.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

namespace luvk
{
    class LUVKMODULE_API Synchronization : public IRenderModule
    {
    public:
        struct FrameData
        {
            VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
            luvk::Vector<VkCommandBuffer> SecondaryBuffers{};
            VkSemaphore ImageAvailable{VK_NULL_HANDLE};
            VkFence InFlight{VK_NULL_HANDLE};
            bool Submitted{false};
        };

    private:
        luvk::Vector<FrameData> m_Frames{};
        luvk::Vector<VkSemaphore> m_RenderFinished{};
        CommandBufferPool m_SecondaryPool{};
        std::size_t m_CurrentFrame{0};
        std::size_t m_ThreadCount{1};
        std::shared_ptr<IRenderModule> m_DeviceModule{};

    public:
        constexpr Synchronization() = default;
        ~Synchronization() override = default;

        void Initialize(std::shared_ptr<IRenderModule> const& DeviceModule, std::size_t FrameCount);

        void SetupFrames(std::shared_ptr<IRenderModule> const& DeviceModule,
                         std::shared_ptr<IRenderModule> const& SwapChainModule,
                         std::shared_ptr<IRenderModule> const& CommandPoolModule);

        [[nodiscard]] constexpr std::size_t GetFrameCount() const
        {
            return std::size(m_Frames);
        }

        [[nodiscard]] constexpr FrameData& GetFrame(const std::size_t Index)
        {
            return m_Frames.at(Index);
        }

        [[nodiscard]] constexpr VkSemaphore& GetRenderFinished(const std::size_t Index)
        {
            return m_RenderFinished.at(Index);
        }

        [[nodiscard]] constexpr std::size_t GetCurrentFrame() const
        {
            return m_CurrentFrame;
        }

        [[nodiscard]] constexpr std::size_t GetThreadCount() const
        {
            return m_ThreadCount;
        }

        [[nodiscard]] constexpr CommandBufferPool& GetSecondaryPool()
        {
            return m_SecondaryPool;
        }

        constexpr void AdvanceFrame()
        {
            m_CurrentFrame = (m_CurrentFrame + 1) % std::size(m_Frames);
        }

    private:
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override {}
        void ClearResources() override;
    };
} // namespace luvk
