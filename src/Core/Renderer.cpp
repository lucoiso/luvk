// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Renderer.hpp"
#include "luvk/Libraries/Compile.hpp"

luvk::Renderer::Renderer()
{
    InitializeDependencies();
}

luvk::ExtensionNameArray luvk::Renderer::GetEnabledLayersNames() const
{
    ExtensionNameArray Output {};

    for (auto const& [Enabled, Name, Extensions] : m_Layers)
    {
        const char *LayerData = std::data(Name);

        if (!Enabled || Output.Contains(LayerData))
        {
            continue;
        }

        Output.Emplace(LayerData);
    }

    return Output;
}

luvk::ExtensionNameArray luvk::Renderer::GetEnabledExtensionsNames() const
{
    ExtensionNameArray Output {};

    for (auto const& [LayerEnabled, LayerName, LayerExtensions] : m_Layers)
    {
        if (!LayerEnabled)
        {
            continue;
        }

        for (auto const& [ExtensionName, ExtensionEnabled] : LayerExtensions)
        {
            const char *ExtData = std::data(ExtensionName);

            if (!ExtensionEnabled || Output.Contains(ExtData))
            {
                continue;
            }

            Output.Emplace(ExtData);
        }
    }

    return Output;
}

void luvk::Renderer::InitializeDependencies()
{
    if (static bool IsVolkInitialized = false;
        !IsVolkInitialized)
    {
        volkInitialize();
        IsVolkInitialized = true;
    }

    FetchAvailableLayers();
}

bool luvk::Renderer::InitializeRenderer(InstanceCreationArguments const& Arguments)
{
    VkApplicationInfo const AppInfo{.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                                    .pApplicationName = std::data(Arguments.ApplicationName),
                                    .applicationVersion = Arguments.ApplicationVersion,
                                    .pEngineName = std::data(Arguments.EngineName),
                                    .engineVersion = Arguments.EngineVersion,
                                    .apiVersion = VK_API_VERSION_1_3};

    auto const Layers = GetEnabledLayersNames();
    auto const Extensions = GetEnabledExtensionsNames();

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

std::vector<luvk::Pair<std::string, bool>> luvk::Renderer::FetchAvailableLayerExtensions(std::string_view const LayerName)
{
    std::uint32_t NumExtensions = 0U;
    vkEnumerateInstanceExtensionProperties(std::data(LayerName), &NumExtensions, nullptr);

    std::vector<VkExtensionProperties> ExtensionProperties(NumExtensions);
    vkEnumerateInstanceExtensionProperties(std::data(LayerName), &NumExtensions, std::data(ExtensionProperties));

    std::vector<Pair<std::string, bool>> Output;
    Output.reserve(NumExtensions);

    for (auto const&[ExtensionName,
                     ExtensionSpecVersion] : ExtensionProperties)
    {
        Output.push_back(Pair<std::string, bool> { ExtensionName, false });
    }

    return Output;
}

void luvk::Renderer::FetchAvailableLayers()
{
    std::uint32_t NumLayers = 0U;
    vkEnumerateInstanceLayerProperties(&NumLayers, nullptr);

    std::vector<VkLayerProperties> LayerProperties(NumLayers);
    vkEnumerateInstanceLayerProperties(&NumLayers, std::data(LayerProperties));

    for (auto &[LayerName,
                LayerSpecVersion,
                LayerImplementationVersion,
                LayerDescription] : LayerProperties)
    {
        Layer NewLayer { .Name = LayerName, .Extensions = {} };
        auto AvailableExtensions = FetchAvailableLayerExtensions(LayerName);
        std::ranges::copy(AvailableExtensions, std::back_inserter(NewLayer.Extensions));

        m_Layers.Emplace(std::move(NewLayer));
    }
}
