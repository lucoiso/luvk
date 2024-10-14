// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Renderer.hpp"
#include "luvk/Core/Device.hpp"
#include "luvk/Core/RenderGraph.hpp"

static bool s_IsVolkInitialized = false;

luvk::Renderer::~Renderer()
{
    // TODO : Add shutdown logic
}

void luvk::Renderer::PreInitializeRenderer()
{
    if (!s_IsVolkInitialized)
    {
        volkInitialize();
        s_IsVolkInitialized = true;
    }

    m_Extensions.FillExtensionsContainer();
}

bool luvk::Renderer::InitializeRenderer(InstanceCreationArguments const& Arguments)
{
    if (!s_IsVolkInitialized)
    {
        throw std::runtime_error("Volk is not initialized, ensure that you call PreInitializeRenderer() before executing this function.");
    }

    VkApplicationInfo const AppInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName = std::data(Arguments.ApplicationName),
                                    .applicationVersion = Arguments.ApplicationVersion,
                                    .pEngineName = std::data(Arguments.EngineName),
                                    .engineVersion = Arguments.EngineVersion,
                                    .apiVersion = VK_API_VERSION_1_3};

    auto const Layers = m_Extensions.GetEnabledLayersNames();
    auto Extensions = m_Extensions.GetEnabledExtensionsNames();

    Extensions.insert(std::end(Extensions), Arguments.ExtraExtensions,Arguments.ExtraExtensions + Arguments.ExtraExtensionsSize);

    VkInstanceCreateInfo const InstanceCreateInfo {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                   .pNext = nullptr,
                                                   .pApplicationInfo = &AppInfo,
                                                   .enabledLayerCount = static_cast<std::uint32_t>(std::size(Layers)),
                                                   .ppEnabledLayerNames = std::data(Layers),
                                                   .enabledExtensionCount = static_cast<std::uint32_t>(std::size(Extensions)),
                                                   .ppEnabledExtensionNames = std::data(Extensions)};

    vkCreateInstance(&InstanceCreateInfo, nullptr, &m_Instance);

    if (m_Instance != nullptr)
    {
        volkLoadInstance(m_Instance);
        return true;
    }

    return false;
}

void luvk::Renderer::PostInitializeRenderer()
{
    InitializeDependencies(nullptr);
}

bool luvk::Renderer::DoInitialization(InstanceCreationArguments const &Arguments)
{
    PreInitializeRenderer();
    if (InitializeRenderer(Arguments))
    {
        PostInitializeRenderer();
        return true;
    }

    return false;
}

void luvk::Renderer::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    m_RenderModules.at(0U) = std::make_shared<luvk::Device>();
    m_RenderModules.at(1U) = std::make_shared<luvk::RenderGraph>();

    std::for_each(std::execution::seq,
                  std::begin(m_RenderModules),
                  std::end(m_RenderModules),
                  [this] (const auto& ModuleIt)
                  {
                        ModuleIt->InitializeDependencies(shared_from_this());
                  });
}
