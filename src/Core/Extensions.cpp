// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Extensions.hpp"

void luvk::IExtensions::SetLayerState(std::string_view const Layer, bool const State)
{
    for (auto& [Enabled, Name, Extensions] : m_Layers)
    {
        if (std::equal(std::execution::unseq,
                       std::cbegin(Name),
                       std::cend(Name),
                       std::cbegin(Layer),
                       std::cend(Layer)))
        {
            Enabled = State;
            break;
        }
    }
}

void luvk::IExtensions::SetExtensionState(std::string_view const FromLayer, std::string_view const Extension, bool const State)
{
    for (auto& [LayerEnabled, LayerName, LayerExtensions] : m_Layers)
    {
        if (std::equal(std::execution::unseq,
                       std::cbegin(LayerName),
                       std::cend(LayerName),
                       std::cbegin(FromLayer),
                       std::cend(FromLayer)))
        {
            for (auto& [ExtensionName, ExtensionEnabled] : LayerExtensions)
            {
                if (std::equal(std::execution::unseq,
                               std::cbegin(ExtensionName),
                               std::cend(ExtensionName),
                               std::cbegin(Extension),
                               std::cend(Extension)))
                {
                    ExtensionEnabled = State;
                    return;
                }
            }
        }
    }
}


luvk::ExtensionNameArray luvk::IExtensions::GetEnabledLayersNames() const
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

luvk::ExtensionNameArray luvk::IExtensions::GetEnabledExtensionsNames() const
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

void luvk::IExtensions::FillExtensionsContainer()
{
    if (!m_Layers.empty())
    {
        m_Layers.clear();
    }

    LayerPropertyArray const AvailableProperties = FetchAvailableLayers();

    std::for_each(std::execution::unseq,
                  std::begin(AvailableProperties),
                  std::end(AvailableProperties),
                  [this] (VkLayerProperties const& Iterator)
                  {
                        ExtensionPropertyArray const AvailableExtensions = FetchAvailableLayerExtensions(Iterator.layerName);

                        Layer NewLayer { .Name = Iterator.layerName, .Extensions = {} };
                        NewLayer.Extensions.reserve(std::size(AvailableExtensions));

                        std::for_each(std::execution::unseq,
                                      std::begin(AvailableExtensions),
                                      std::end(AvailableExtensions),
                                      [&NewLayer] (VkExtensionProperties const& Iterator)
                                      {
                                         NewLayer.Extensions.push_back({ Iterator.extensionName, false });
                                      });

                        m_Layers.Emplace(std::move(NewLayer));
                  });
}

luvk::ExtensionPropertyArray luvk::InstanceExtensions::FetchAvailableLayerExtensions(std::string_view const LayerName) const
{
    std::uint32_t NumExtensions = 0U;
    vkEnumerateInstanceExtensionProperties(std::data(LayerName), &NumExtensions, nullptr);

    ExtensionPropertyArray ExtensionProperties(NumExtensions);
    vkEnumerateInstanceExtensionProperties(std::data(LayerName), &NumExtensions, std::data(ExtensionProperties));

    return ExtensionProperties;
}

luvk::LayerPropertyArray luvk::InstanceExtensions::FetchAvailableLayers() const
{
    std::uint32_t NumLayers = 0U;
    vkEnumerateInstanceLayerProperties(&NumLayers, nullptr);

    LayerPropertyArray Output(NumLayers);
    vkEnumerateInstanceLayerProperties(&NumLayers, std::data(Output));

    return Output;
}

luvk::ExtensionPropertyArray luvk::DeviceExtensions::FetchAvailableLayerExtensions(std::string_view const LayerName) const
{
    std::uint32_t NumExtensions = 0U;
    vkEnumerateDeviceExtensionProperties(m_Device, std::data(LayerName), &NumExtensions, nullptr);

    ExtensionPropertyArray ExtensionProperties(NumExtensions);
    vkEnumerateDeviceExtensionProperties(m_Device, std::data(LayerName), &NumExtensions, std::data(ExtensionProperties));

    return ExtensionProperties;
}

luvk::LayerPropertyArray luvk::DeviceExtensions::FetchAvailableLayers() const
{
    std::uint32_t NumLayers = 0U;
    vkEnumerateDeviceLayerProperties(m_Device, &NumLayers, nullptr);

    LayerPropertyArray Output(NumLayers);
    vkEnumerateDeviceLayerProperties(m_Device, &NumLayers, std::data(Output));

    return Output;
}
