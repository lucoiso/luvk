// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Extensions.hpp"
#include "luvk/Libraries/Functional.hpp"

constexpr auto g_ReservationSize = 256U;

void luvk::IExtensions::SetLayerState(std::string_view const Layer, bool const State, bool const EnableAllExtensions)
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

            return;
        }
    }

    throw std::runtime_error("Unavailable layer '" + std::string(Layer) + "'");
}

void luvk::IExtensions::SetExtensionState(std::string_view const FromLayer, std::string_view const Extension, bool const State)
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
                    return;
                }
            }
        }
    }

    throw std::runtime_error("Unavailable extension: " + std::string(Extension));
}

std::vector<const char*> luvk::IExtensions::GetEnabledLayersNames() const
{
    std::vector<const char*> Output{};
    Output.reserve(g_ReservationSize);

    for (auto const& [Enabled, Name, Extensions] : m_Layers)
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

std::vector<const char*> luvk::IExtensions::GetEnabledExtensionsNames() const
{
    std::vector<const char*> Output{};
    Output.reserve(g_ReservationSize);

    for (auto const& [LayerEnabled, LayerName, LayerExtensions] : m_Layers)
    {
        if (!LayerEnabled)
        {
            continue;
        }

        for (auto const& [ExtensionName, ExtensionEnabled] : LayerExtensions)
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

    std::vector<VkLayerProperties> const AvailableProperties = FetchAvailableLayers();

    std::for_each(std::execution::unseq,
                  std::begin(AvailableProperties),
                  std::end(AvailableProperties),
                  [this](VkLayerProperties const& Iterator)
                  {
                      std::vector<VkExtensionProperties> const AvailableExtensions = FetchAvailableLayerExtensions(Iterator.layerName);

                      Layer NewLayer{.Name = Iterator.layerName, .Extensions = {}};
                      NewLayer.Extensions.reserve(g_ReservationSize);

                      std::for_each(std::execution::unseq,
                                    std::begin(AvailableExtensions),
                                    std::end(AvailableExtensions),
                                    [&NewLayer](VkExtensionProperties const& Iterator)
                                    {
                                        NewLayer.Extensions.push_back({Iterator.extensionName, false});
                                    });

                      m_Layers.push_back(std::move(NewLayer));
                  });
}

std::vector<VkExtensionProperties> luvk::InstanceExtensions::FetchAvailableLayerExtensions(std::string_view const LayerName) const
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

std::vector<VkExtensionProperties> luvk::DeviceExtensions::FetchAvailableLayerExtensions(std::string_view const LayerName) const
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
