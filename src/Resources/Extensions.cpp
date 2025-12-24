// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Extensions.hpp"
#include <cstdint>
#include <ostream>

constexpr auto g_ReservationSize = 256U;

bool luvk::IExtensions::SetLayerState(const std::string_view Layer, const bool State, const bool EnableAllExtensions)
{
    for (auto& [Enabled, Name, Extensions] : m_Layers)
    {
        if (std::ranges::equal(std::as_const(Name), Layer))
        {
            Enabled = State;

            if (State && EnableAllExtensions)
            {
                std::ranges::for_each(Extensions,
                                      [](auto& ExtensionIt)
                                      {
                                          ExtensionIt.second = true;
                                      });
            }

            return true;
        }
    }

    return false;
}

bool luvk::IExtensions::SetExtensionState(const std::string_view FromLayer, const std::string_view Extension, const bool State)
{
    for (auto& [LayerEnabled, LayerName, LayerExtensions] : m_Layers)
    {
        if (std::ranges::equal(std::as_const(LayerName), FromLayer))
        {
            for (auto& [ExtensionName, ExtensionEnabled] : LayerExtensions)
            {
                if (std::ranges::equal(std::as_const(ExtensionName), Extension))
                {
                    if (State)
                    {
                        LayerEnabled = true;
                    }

                    ExtensionEnabled = State;
                    return true;
                }
            }
        }
    }

    return false;
}

std::vector<const char*> luvk::IExtensions::GetEnabledLayersNames() const
{
    std::vector<const char*> Output{};
    Output.reserve(g_ReservationSize);

    for (const auto& [Enabled, Name, Extensions] : m_Layers)
    {
        const char* LayerData = std::data(Name);

        if (std::empty(Name) || !Enabled || std::ranges::contains(Output, LayerData))
        {
            continue;
        }

        Output.push_back(LayerData);
    }

    return Output;
}

std::vector<const char*> luvk::IExtensions::GetEnabledExtensionsNames() const
{
    std::vector<const char*> Output{};
    Output.reserve(g_ReservationSize);

    for (const auto& [LayerEnabled, LayerName, LayerExtensions] : m_Layers)
    {
        if (!LayerEnabled)
        {
            continue;
        }

        for (const auto& [ExtensionName, ExtensionEnabled] : LayerExtensions)
        {
            const char* ExtData = std::data(ExtensionName);

            if (std::empty(ExtensionName) || !ExtensionEnabled || std::ranges::contains(Output, ExtData))
            {
                continue;
            }

            Output.push_back(ExtData);
        }
    }

    return Output;
}

void luvk::IExtensions::FillExtensionsContainer()
{
    if (!std::empty(m_Layers))
    {
        m_Layers.clear();
    }
    m_Layers.reserve(g_ReservationSize);

    const std::vector<VkLayerProperties> AvailableProperties = FetchAvailableLayers();

    std::ranges::for_each(AvailableProperties,
                          [this](const VkLayerProperties& Iterator)
                          {
                              const std::vector<VkExtensionProperties> AvailableExtensions = FetchAvailableLayerExtensions(Iterator.layerName);

                              Layer NewLayer{.Name = Iterator.layerName, .Extensions = {}};
                              NewLayer.Extensions.reserve(g_ReservationSize);

                              std::ranges::for_each(AvailableExtensions,
                                                    [&NewLayer](const VkExtensionProperties& ExtIt)
                                                    {
                                                        NewLayer.Extensions.emplace_back(ExtIt.extensionName, false);
                                                    });

                              m_Layers.push_back(std::move(NewLayer));
                          });
}

std::vector<VkExtensionProperties> luvk::InstanceExtensions::FetchAvailableLayerExtensions(const std::string_view LayerName) const
{
    std::uint32_t NumExtensions = 0U;
    vkEnumerateInstanceExtensionProperties(std::data(LayerName), &NumExtensions, nullptr);

    std::vector<VkExtensionProperties> ExtensionProperties(NumExtensions);
    vkEnumerateInstanceExtensionProperties(std::data(LayerName), &NumExtensions, std::data(ExtensionProperties));

    return ExtensionProperties;
}

std::vector<VkLayerProperties> luvk::InstanceExtensions::FetchAvailableLayers() const
{
    std::uint32_t NumLayers = 0U;
    vkEnumerateInstanceLayerProperties(&NumLayers, nullptr);

    std::vector<VkLayerProperties> Output(NumLayers + 1U);
    vkEnumerateInstanceLayerProperties(&NumLayers, std::data(Output));

    return Output;
}

std::vector<VkExtensionProperties> luvk::DeviceExtensions::FetchAvailableLayerExtensions(const std::string_view LayerName) const
{
    std::uint32_t NumExtensions = 0U;
    vkEnumerateDeviceExtensionProperties(m_Device, std::data(LayerName), &NumExtensions, nullptr);

    std::vector<VkExtensionProperties> ExtensionProperties(NumExtensions);
    vkEnumerateDeviceExtensionProperties(m_Device, std::data(LayerName), &NumExtensions, std::data(ExtensionProperties));

    return ExtensionProperties;
}

std::vector<VkLayerProperties> luvk::DeviceExtensions::FetchAvailableLayers() const
{
    std::uint32_t NumLayers = 0U;
    vkEnumerateDeviceLayerProperties(m_Device, &NumLayers, nullptr);

    std::vector<VkLayerProperties> Output(NumLayers + 1U);
    vkEnumerateDeviceLayerProperties(m_Device, &NumLayers, std::data(Output));

    return Output;
}
