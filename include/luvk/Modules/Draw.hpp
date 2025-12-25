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

    struct LUVK_API DrawCallbackInfo
    {
        std::function<bool(VkCommandBuffer)> Callback;
    };

    class LUVK_API Draw : public IRenderModule
    {
    protected:
        std::array<VkClearValue, 2U> m_ClearValues{VkClearValue{.color = {0.2F, 0.2F, 0.2F, 1.F}},
                                                   VkClearValue{.depthStencil = {1.F, 0}}};

        std::vector<DrawCallbackInfo> m_PreRenderCallbacks{};
        std::vector<DrawCallbackInfo> m_DrawCallbacks{};
        std::vector<DrawCallbackInfo> m_PostRenderCallbacks{};

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

        void RegisterPreRenderCommand(DrawCallbackInfo&& Cmd)
        {
            m_PreRenderCallbacks.emplace_back(std::move(Cmd));
        }

        void RegisterDrawCommand(DrawCallbackInfo&& Cmd)
        {
            m_DrawCallbacks.emplace_back(std::move(Cmd));
        }

        void RegisterPostRenderCommand(DrawCallbackInfo&& Cmd)
        {
            m_PostRenderCallbacks.emplace_back(std::move(Cmd));
        }

        void RecordCommands(const FrameData& Frame, std::uint32_t ImageIndex);
        void SubmitFrame(FrameData& Frame, std::uint32_t ImageIndex) const;
    };
} // namespace luvk
