// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <memory>
#include <span>
#include <volk.h>
#include "luvk/Constants/Rendering.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Types/FrameData.hpp"

namespace luvk
{
    class Device;
    class SwapChain;
    class CommandPool;

    class LUVK_API Synchronization : public IRenderModule
    {
    protected:
        std::array<FrameData, Constants::ImageCount>   m_Frames{};
        std::array<VkSemaphore, Constants::ImageCount> m_RenderFinished{};
        std::size_t                                    m_CurrentFrame{0};
        std::shared_ptr<Device>                        m_DeviceModule{};

    public:
        Synchronization() = delete;
        Synchronization(const std::shared_ptr<Device>& DeviceModule);

        ~Synchronization() override
        {
            Synchronization::ClearResources();
        }

        void Initialize(std::span<const VkCommandBuffer, Constants::ImageCount> CommandBuffers);
        void ResetFrames();

        [[nodiscard]] constexpr FrameData& GetFrame(const std::size_t Index) noexcept
        {
            return m_Frames.at(Index);
        }

        [[nodiscard]] constexpr FrameData& GetCurrentFrameData() noexcept
        {
            return m_Frames.at(m_CurrentFrame);
        }

        [[nodiscard]] constexpr VkSemaphore GetRenderFinished(const std::size_t Index) const noexcept
        {
            return m_RenderFinished.at(Index);
        }

        [[nodiscard]] constexpr std::size_t GetCurrentFrame() const noexcept
        {
            return m_CurrentFrame;
        }

        constexpr void AdvanceFrame() noexcept
        {
            m_CurrentFrame = (m_CurrentFrame + 1) % std::size(m_Frames);
        }

        void WaitFrame(const FrameData& Frame, VkBool32 WaitAll, std::uint64_t Timeout) const;
        void WaitFrame(std::size_t Index, VkBool32 WaitAll, std::uint64_t Timeout) const;
        void WaitCurrentFrame(VkBool32 WaitAll, std::uint64_t Timeout) const;

    protected:
        void ClearResources() override;
    };
} // namespace luvk
