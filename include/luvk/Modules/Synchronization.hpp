// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <iterator>
#include <memory>
#include <volk/volk.h>
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Resources/CommandBufferPool.hpp"
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
        std::size_t m_FrameCount{0};
        std::size_t m_CurrentFrame{0};
        std::shared_ptr<CommandBufferPool> m_SecondaryPool{};
        std::shared_ptr<Device> m_DeviceModule{};
        std::shared_ptr<SwapChain> m_SwapChainModule{};
        std::shared_ptr<CommandPool> m_CommandPoolModule{};

    public:
        Synchronization() = delete;
        Synchronization(const std::shared_ptr<Device>& DeviceModule,
                        const std::shared_ptr<SwapChain>& SwapChainModule,
                        const std::shared_ptr<CommandPool>& CommandPoolModule,
                        std::size_t FrameCount);

        ~Synchronization() override
        {
            Synchronization::ClearResources();
        }

        void Initialize();
        void SetupFrames();

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

        [[nodiscard]] constexpr const std::shared_ptr<CommandBufferPool>& GetSecondaryPool()
        {
            return m_SecondaryPool;
        }

        constexpr void AdvanceFrame()
        {
            m_CurrentFrame = (m_CurrentFrame + 1) % std::size(m_Frames);
        }

    protected:
        void ClearResources() override;
    };
} // namespace luvk
