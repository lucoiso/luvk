// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Libraries/Compile.hpp"
#include "luvk/Types/Layer.hpp"

#include <execution>
#include <string>
#include <vector>
#include <Volk/volk.h>

namespace luvk
{
    constexpr std::size_t LayersCapacity = 512U;

    using LayerArray = Array<Layer, LayersCapacity>;
    using ExtensionNameArray = Array<const char*, LayersCapacity>;

    using ExtensionPropertyArray = std::vector<VkExtensionProperties>;
    using LayerPropertyArray = std::vector<VkLayerProperties>;

    /** Interface for managing device or instance extensions */
    class LUVKMODULE_API IExtensions
    {
        LayerArray m_Layers {};

    public:
        constexpr IExtensions() = default;
        virtual ~IExtensions() = default;

        /** Check if the layer with the given name is available in the system */
        [[nodiscard]] constexpr bool HasAvailableLayer(std::string_view const LayerName) const noexcept
        {
            auto const CompareLayerName = [&LayerName] (Layer const& Iterator)
            {
                return std::equal(std::execution::unseq,
                                  std::cbegin(Iterator.Name),
                                  std::cend(Iterator.Name),
                                  std::cbegin(LayerName),
                                  std::cend(LayerName));
            };

            return std::find_if(std::execution::unseq,
                                std::cbegin(m_Layers),
                                std::cend(m_Layers),
                                CompareLayerName) != std::cend(m_Layers);
        }

        /** Check if the extension with the given name is available in the system */
        [[nodiscard]] constexpr bool HasAvailableExtension(std::string_view const ExtensionName) const noexcept
        {
            auto const CompareExtensionName = [&ExtensionName] (Pair<std::string, bool> const& Iterator)
            {
                return std::equal(std::execution::unseq,
                                  std::cbegin(Iterator.First),
                                  std::cend(Iterator.First),
                                  std::cbegin(ExtensionName),
                                  std::cend(ExtensionName));
            };

            auto const CheckLayerExtensions = [&CompareExtensionName] (Layer const& Iterator)
            {
                return std::find_if(std::execution::unseq,
                                    std::cbegin(Iterator.Extensions),
                                    std::cend(Iterator.Extensions),
                                    CompareExtensionName) != std::cend(Iterator.Extensions);
            };

            return std::find_if(std::execution::unseq,
                                std::cbegin(m_Layers),
                                std::cend(m_Layers),
                                CheckLayerExtensions) != std::cend(m_Layers);
        }

        /** Get the available layers */
        [[nodiscard]] inline luvk::LayerArray const& GetLayers() const
        {
            return m_Layers;
        }

        /** Set the state of the layer */
        void SetLayerState(std::string_view Layer, bool State);

        /** Get the available layers */
        void SetExtensionState(std::string_view FromLayer, std::string_view Extension, bool State);

        /** Get the enabled layers */
        [[nodiscard]] luvk::ExtensionNameArray GetEnabledLayersNames() const;

        /** Get the enabled extensions */
        [[nodiscard]] luvk::ExtensionNameArray GetEnabledExtensionsNames() const;

        /** Fill the extensions container */
        void FillExtensionsContainer();

    protected:
        /** Check for available extensions in the given layer */
        [[nodiscard]] virtual ExtensionPropertyArray FetchAvailableLayerExtensions(std::string_view LayerName) const = 0;

        /** Check for available layers and fill the layers container */
        [[nodiscard]] virtual LayerPropertyArray FetchAvailableLayers() const = 0;
    };

    /** Object that will manage instance extensions */
    class LUVKMODULE_API InstanceExtensions : public IExtensions
    {
    public:
        constexpr InstanceExtensions() = default;
        ~InstanceExtensions() override = default;

    protected:
        /** Check for available extensions in the given layer */
        [[nodiscard]] ExtensionPropertyArray FetchAvailableLayerExtensions(std::string_view LayerName) const override;

        /** Check for available layers and fill the layers container */
        [[nodiscard]] LayerPropertyArray FetchAvailableLayers() const override;
    };

    /** Object that will manage device extensions */
    class LUVKMODULE_API DeviceExtensions : public IExtensions
    {
        VkPhysicalDevice const& m_Device {};

    public:
        DeviceExtensions() = delete;
        constexpr explicit DeviceExtensions(VkPhysicalDevice const& Device) : m_Device(Device) {}
        ~DeviceExtensions() override = default;

    protected:
        /** Check for available extensions in the given layer */
        [[nodiscard]] ExtensionPropertyArray FetchAvailableLayerExtensions(std::string_view LayerName) const override;

        /** Check for available layers and fill the layers container */
        [[nodiscard]] LayerPropertyArray FetchAvailableLayers() const override;
    };
} // namespace luvk