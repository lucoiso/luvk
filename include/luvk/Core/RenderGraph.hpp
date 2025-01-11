// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <rps/runtime/vk/rps_vk_runtime.h>
#include <string_view>

namespace luvk
{
    /** Render module responsible for the Render Graph using AMD's RPS */
    class LUVKMODULE_API RenderGraph : public IRenderModule
    {
        RpsDevice m_Device {RPS_NULL_HANDLE};
        RpsRenderGraph m_RenderGraph {RPS_NULL_HANDLE};

    public:
        constexpr RenderGraph() = default;
        ~RenderGraph() override = default;

        /** Initialize the render graph device */
        void InitializeRPSDevice(std::shared_ptr<IRenderModule> const& DeviceModule);

        /** Create the render graph object */
        void CreateRenderGraph(std::vector<RpsQueueFlags>&& QueueFlags, RpsRpslEntry Entry);

        /** Bind the specified node */
        void BindNode(std::string_view Name, PFN_rpsCmdCallback Callback, void* Context = nullptr, RpsCmdCallbackFlags Flags = RPS_CMD_CALLBACK_FLAG_NONE) const;

        /** Get the RPS device */
        [[nodiscard]] inline RpsDevice const& GetDevice() const
        {
            return m_Device;
        }

        /** Get the RPS render graph */
        [[nodiscard]] inline RpsRenderGraph const& GetRenderGraph() const
        {
            return m_RenderGraph;
        }

    protected:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources(IRenderModule* MainRenderer) override;
    };
} // namespace luvk