// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Resources/Extensions.hpp"

#include <ostream>

#include "luvk/Libraries/Functional.hpp"

constexpr auto g_ReservationSize = 256U;

bool luvk::IExtensions::SetLayerState(const std::string_view Layer, const bool State, const bool EnableAllExtensions)
{
    for (auto& [Enabled, Name, Extensions] : m_Layers)
    {
        if (std::equal(std::execution::unseq, std::cbegin(Name), std::cend(Name), std::cbegin(Layer), std::cend(Layer)))
        {
            Enabled = State;

            if (State && EnableAllExtensions)
            {
                std::for_each(std::execution::unseq,
                              std::begin(Extensions),
                              std::end(Extensions),
                              [](auto& ExtensionIt)
                              {
                                  ExtensionIt.Second = true;
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
        if (std::equal(std::execution::unseq, std::cbegin(LayerName), std::cend(LayerName), std::cbegin(FromLayer), std::cend(FromLayer)))
        {
            for (auto& [ExtensionName, ExtensionEnabled] : LayerExtensions)
            {
                if (std::equal(std::execution::unseq, std::cbegin(ExtensionName), std::cend(ExtensionName), std::cbegin(Extension), std::cend(Extension)))
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

luvk::Vector<const char*> luvk::IExtensions::GetEnabledLayersNames() const
{
    Vector<const char*> Output{};
    Output.reserve(g_ReservationSize);

    for (const auto& [Enabled, Name, Extensions] : m_Layers)
    {
        const char* LayerData = std::data(Name);

        if (std::empty(Name) || !Enabled || Contains(Output, LayerData))
        {
            continue;
        }

        Output.push_back(LayerData);
    }

    return Output;
}

luvk::Vector<const char*> luvk::IExtensions::GetEnabledExtensionsNames() const
{
    Vector<const char*> Output{};
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

            if (std::empty(ExtensionName) || !ExtensionEnabled || Contains(Output, ExtData))
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

    const Vector<VkLayerProperties> AvailableProperties = FetchAvailableLayers();

    std::for_each(std::execution::unseq,
                  std::begin(AvailableProperties),
                  std::end(AvailableProperties),
                  [this](const VkLayerProperties& Iterator)
                  {
                      const Vector<VkExtensionProperties> AvailableExtensions = FetchAvailableLayerExtensions(Iterator.layerName);

                      Layer NewLayer{.Name = Iterator.layerName, .Extensions = {}};
                      NewLayer.Extensions.reserve(g_ReservationSize);

                      std::for_each(std::execution::unseq,
                                    std::begin(AvailableExtensions),
                                    std::end(AvailableExtensions),
                                    [&NewLayer](const VkExtensionProperties& ExtIt)
                                    {
                                        NewLayer.Extensions.push_back({ExtIt.extensionName, false});
                                    });

                      m_Layers.push_back(std::move(NewLayer));
                  });
}

luvk::Vector<VkExtensionProperties> luvk::InstanceExtensions::FetchAvailableLayerExtensions(const std::string_view LayerName) const
{
    std::uint32_t NumExtensions = 0U;
    vkEnumerateInstanceExtensionProperties(std::data(LayerName), &NumExtensions, nullptr);

    Vector<VkExtensionProperties> ExtensionProperties(NumExtensions);
    vkEnumerateInstanceExtensionProperties(std::data(LayerName), &NumExtensions, std::data(ExtensionProperties));

    return ExtensionProperties;
}

luvk::Vector<VkLayerProperties> luvk::InstanceExtensions::FetchAvailableLayers() const
{
    std::uint32_t NumLayers = 0U;
    vkEnumerateInstanceLayerProperties(&NumLayers, nullptr);

    Vector<VkLayerProperties> Output(NumLayers + 1U);
    vkEnumerateInstanceLayerProperties(&NumLayers, std::data(Output));

    return Output;
}

luvk::Vector<VkExtensionProperties> luvk::DeviceExtensions::FetchAvailableLayerExtensions(const std::string_view LayerName) const
{
    std::uint32_t NumExtensions = 0U;
    vkEnumerateDeviceExtensionProperties(m_Device, std::data(LayerName), &NumExtensions, nullptr);

    Vector<VkExtensionProperties> ExtensionProperties(NumExtensions);
    vkEnumerateDeviceExtensionProperties(m_Device, std::data(LayerName), &NumExtensions, std::data(ExtensionProperties));

    return ExtensionProperties;
}

luvk::Vector<VkLayerProperties> luvk::DeviceExtensions::FetchAvailableLayers() const
{
    std::uint32_t NumLayers = 0U;
    vkEnumerateDeviceLayerProperties(m_Device, &NumLayers, nullptr);

    Vector<VkLayerProperties> Output(NumLayers + 1U);
    vkEnumerateDeviceLayerProperties(m_Device, &NumLayers, std::data(Output));

    return Output;
}
