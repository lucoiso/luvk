/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <functional>
#include <span>
#include <vector>
#include <volk.h>
#include "luvk/Interfaces/IRenderModule.hpp"
#include "luvk/Types/FrameData.hpp"
#include "luvk/Types/RenderTarget.hpp"

namespace luvk
{
    class Device;
    class Synchronization;

    struct LUVK_API DrawCallbackInfo
    {
        std::function<bool(VkCommandBuffer)> Callback;
    };

    class LUVK_API Draw : public IRenderModule
    {
    protected:
        std::vector<DrawCallbackInfo> m_BeginFrameCallbacks{};
        std::vector<DrawCallbackInfo> m_RecordFrameCallbacks{};

        std::shared_ptr<Device>          m_DeviceModule{};
        std::shared_ptr<Synchronization> m_SyncModule{};

    public:
        Draw() = delete;
        explicit Draw(const std::shared_ptr<Device>& DeviceModule, const std::shared_ptr<Synchronization>& SyncModule);

        ~Draw() override = default;

        void RegisterOnBeginFrame(DrawCallbackInfo&& Cmd)
        {
            m_BeginFrameCallbacks.emplace_back(std::move(Cmd));
        }

        void RegisterOnRecordFrame(DrawCallbackInfo&& Cmd)
        {
            m_RecordFrameCallbacks.emplace_back(std::move(Cmd));
        }

        void RecordCommands(const FrameData& Frame, const RenderTarget& Target);
        void SubmitFrame(FrameData& Frame, std::uint32_t ImageIndex) const;
    };
}
