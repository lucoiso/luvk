// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Renderer.hpp"

static bool s_IsVolkInitialized = false;

luvk::Renderer::~Renderer()
{
    Renderer::ClearResources(nullptr);
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

bool luvk::Renderer::InitializeRenderer(InstanceCreationArguments const& Arguments, void* const& pNext)
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
    auto const Extensions = m_Extensions.GetEnabledExtensionsNames();

    VkInstanceCreateInfo const InstanceCreateInfo {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                                   .pNext = pNext,
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

void luvk::Renderer::PostInitializeRenderer(std::vector<std::shared_ptr<IRenderModule>> &&Modules)
{
    m_RenderModules.resize(std::size(Modules));

    std::move(std::execution::seq,
              std::begin(Modules),
              std::end(Modules),
              std::begin(m_RenderModules));

    InitializeDependencies(nullptr);
}

void luvk::Renderer::InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer)
{
    std::for_each(std::execution::seq,
                  std::begin(m_RenderModules),
                  std::end(m_RenderModules),
                  [this] (const auto& ModuleIt)
                  {
                      ModuleIt->InitializeDependencies(shared_from_this());
                  });
}

void luvk::Renderer::ClearResources(IRenderModule* const MainRenderer)
{
    std::for_each(std::execution::seq,
                  std::rbegin(m_RenderModules),
                  std::rend(m_RenderModules),
                  [this] (std::shared_ptr<IRenderModule> const& ModuleIt)
                  {
                      ModuleIt->ClearResources(this);
                  });

    if (m_Instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_Instance, nullptr);
    }

    if (s_IsVolkInitialized)
    {
        volkFinalize();
    }
}
