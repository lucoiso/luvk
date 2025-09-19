// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <iterator>
#include <memory>
#include <volk/volk.h>
#include "luvk/Resources/CommandBufferPool.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class Device;
    class SwapChain;
    class CommandPool;

    class LUVKMODULE_API Synchronization : public IRenderModule
    {
    public:
        struct FrameData
        {
            VkCommandBuffer CommandBuffer{VK_NULL_HANDLE};
            Vector<VkCommandBuffer> SecondaryBuffers{};
            VkSemaphore ImageAvailable{VK_NULL_HANDLE};
            VkFence InFlight{VK_NULL_HANDLE};
            bool Submitted{false};
        };

    private:
        Vector<FrameData> m_Frames{};
        Vector<VkSemaphore> m_RenderFinished{};
        CommandBufferPool m_SecondaryPool{};
        std::size_t m_CurrentFrame{0};
        std::size_t m_ThreadCount{1};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        constexpr Synchronization() = default;
        ~Synchronization() override = default;

        void Initialize(const std::shared_ptr<Device>& DeviceModule, std::size_t FrameCount);

        void SetupFrames(const std::shared_ptr<SwapChain>& SwapChainModule,
                         const std::shared_ptr<CommandPool>& CommandPoolModule);

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
        void InitializeDependencies(const std::shared_ptr<IRenderModule>&) override {}
        void ClearResources() override;
    };
} // namespace luvk
