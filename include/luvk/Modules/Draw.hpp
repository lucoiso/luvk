// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <functional>
#include <span>
#include <vector>
#include <volk.h>
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Device;
    class SwapChain;
    class Synchronization;
    struct FrameData;

    class LUVK_API Draw : public IRenderModule
    {
    protected:
        std::array<VkClearValue, 2U> m_ClearValues{VkClearValue{.color = {0.2F, 0.2F, 0.2F, 1.F}},
                                                   VkClearValue{.depthStencil = {1.F, 0}}};
        std::vector<std::function<void(VkCommandBuffer)>> m_PostRenderCommands{};
        std::function<void(VkCommandBuffer)>              m_PreRenderCallback{nullptr};
        std::function<void(VkCommandBuffer)>              m_DrawCallback{nullptr};

        std::shared_ptr<Device>          m_DeviceModule{};
        std::shared_ptr<SwapChain>       m_SwapChainModule{};
        std::shared_ptr<Synchronization> m_SyncModule{};

    public:
        Draw() = delete;
        explicit Draw(const std::shared_ptr<Device>&          DeviceModule,
                      const std::shared_ptr<SwapChain>&       SwapChainModule,
                      const std::shared_ptr<Synchronization>& SyncModule);

        ~Draw() override = default;

        [[nodiscard]] constexpr std::span<const VkClearValue> GetClearValues() const noexcept
        {
            return m_ClearValues;
        }

        void SetClearValues(const std::span<const VkClearValue, 2> Values)
        {
            std::ranges::copy(Values, std::begin(m_ClearValues));
        }

        void EnqueueCommand(std::function<void(VkCommandBuffer)>&& Cmd)
        {
            m_PostRenderCommands.emplace_back(std::move(Cmd));
        }

        void SetPreRenderCallback(std::function<void(VkCommandBuffer)>&& Callback)
        {
            m_PreRenderCallback = std::move(Callback);
        }

        void SetDrawCallback(std::function<void(VkCommandBuffer)>&& Callback)
        {
            m_DrawCallback = std::move(Callback);
        }

        void RecordCommands(const FrameData& Frame, std::uint32_t ImageIndex);
        void SubmitFrame(FrameData& Frame, std::uint32_t ImageIndex) const;
    };
} // namespace luvk
