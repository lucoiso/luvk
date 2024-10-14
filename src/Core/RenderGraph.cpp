// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/RenderGraph.hpp"
#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Device.hpp"

luvk::RenderGraph::~RenderGraph()
{
    // TODO : Add shutdown logic
}

void luvk::RenderGraph::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    return; // TODO : Remove return. Logical Device isn't created and will crash
    InitializeRPSDevice(dynamic_cast<luvk::Renderer*>(MainRenderer.get())->GetModule(luvk::RenderModuleIndex::DEVICE));
}

void luvk::RenderGraph::InitializeRPSDevice(std::shared_ptr<IRenderModule> const& DeviceModule)
{
    auto const CastedModule = dynamic_cast<luvk::Device*>(DeviceModule.get());

    RpsVKRuntimeDeviceCreateInfo const CreationArguments{.hVkDevice = CastedModule->GetLogicalDevice(),
                                                         .hVkPhysicalDevice = CastedModule->GetPhysicalDevice(),
                                                         .flags = RPS_VK_RUNTIME_FLAG_NONE,
                                                         .pVkFunctions = nullptr};

    RpsResult const Result = rpsVKRuntimeDeviceCreate(&CreationArguments, &m_Device);

    if (Result != RPS_OK)
    {
        throw std::runtime_error("Failed to create the RPS device.");
    }
}
