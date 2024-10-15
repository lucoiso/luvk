// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <rps/runtime/vk/rps_vk_runtime.h>

namespace luvk
{
    /** Render module responsible for the Render Graph using AMD's RPS */
    class LUVKMODULE_API RenderGraph : public IRenderModule
    {
        RpsDevice m_Device {};

    public:
        constexpr RenderGraph() = default;
        ~RenderGraph() override = default;

        /** Initialize the render graph device */
        void InitializeRPSDevice(std::shared_ptr<IRenderModule> const& DeviceModule);

    protected:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources(IRenderModule* MainRenderer) override;
    };
} // namespace luvk