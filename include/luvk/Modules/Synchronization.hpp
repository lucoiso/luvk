// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <memory>
#include <vector>
#include <volk.h>
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Device;
    class SwapChain;
    class CommandPool;

    struct LUVK_API FrameData
    {
        bool                         Submitted{false};
        VkFence                      InFlight{VK_NULL_HANDLE};
        VkSemaphore                  ImageAvailable{VK_NULL_HANDLE};
        VkCommandBuffer              CommandBuffer{VK_NULL_HANDLE};
        std::vector<VkCommandBuffer> SecondaryBuffers{};
    };

    class LUVK_API Synchronization : public IRenderModule
    {
    protected:
        std::array<FrameData, Constants::ImageCount>   m_Frames{};
        std::array<VkSemaphore, Constants::ImageCount> m_RenderFinished{};
        std::size_t                                    m_CurrentFrame{0};
        std::shared_ptr<Device>                        m_DeviceModule{};
        std::shared_ptr<SwapChain>                     m_SwapChainModule{};
        std::shared_ptr<CommandPool>                   m_CommandPoolModule{};

    public:
        Synchronization() = delete;
        Synchronization(const std::shared_ptr<Device>&      DeviceModule,
                        const std::shared_ptr<SwapChain>&   SwapChainModule,
                        const std::shared_ptr<CommandPool>& CommandPoolModule);

        ~Synchronization() override
        {
            Synchronization::ClearResources();
        }

        void Initialize();
        void SetupFrames();

        [[nodiscard]] constexpr FrameData& GetFrame(const std::size_t Index) noexcept
        {
            return m_Frames.at(Index);
        }

        [[nodiscard]] constexpr VkSemaphore GetRenderFinished(const std::size_t Index) const noexcept
        {
            return m_RenderFinished.at(Index);
        }

        [[nodiscard]] constexpr std::size_t GetCurrentFrame() const noexcept
        {
            return m_CurrentFrame;
        }

        constexpr void AdvanceFrame()
        {
            m_CurrentFrame = (m_CurrentFrame + 1) % std::size(m_Frames);
        }

    protected:
        void ClearResources() override;
    };
} // namespace luvk
