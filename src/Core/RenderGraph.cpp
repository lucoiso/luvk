// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/RenderGraph.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Device.hpp"

#include <volk.h>

void luvk::RenderGraph::InitializeRPSDevice(std::shared_ptr<IRenderModule> const& DeviceModule)
{
    auto const CastModule = dynamic_cast<luvk::Device*>(DeviceModule.get());

    #define VOLK_TO_RPS(callName) .##callName = callName,

    static RpsVKFunctions DynamicTable {
        RPS_VK_FUNCTION_TABLE(VOLK_TO_RPS)
    };

    RpsVKRuntimeDeviceCreateInfo const CreationArguments{.hVkDevice = CastModule->GetLogicalDevice(),
                                                         .hVkPhysicalDevice = CastModule->GetPhysicalDevice(),
                                                         .flags = RPS_VK_RUNTIME_FLAG_NONE,
                                                         .pVkFunctions = &DynamicTable};

    if (rpsVKRuntimeDeviceCreate(&CreationArguments, &m_Device) != RPS_OK)
    {
        throw std::runtime_error("Failed to create the RPS device.");
    }
}

void luvk::RenderGraph::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    // Do nothing: Needs Device Module configuration to be set by the user. InitializeRPSDevice needs to be called after creating the logical device.
}

void luvk::RenderGraph::ClearResources(IRenderModule* const MainRenderer)
{
    if (m_Device != RPS_NULL_HANDLE)
    {
        rpsDeviceDestroy(m_Device);
    }
}
