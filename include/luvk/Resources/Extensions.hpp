// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once
/** Extensions.hpp definitions */

#include "luvk/Module.hpp"
#include "luvk/Libraries/CompileTime.hpp"
#include "luvk/Types/Layer.hpp"

#include <execution>
#include <string>
#include <vector>
#include <volk/volk.h>

namespace luvk
{
    /** Interface for managing device or instance extensions */
    class LUVKMODULE_API IExtensions
    {
        std::vector<Layer> m_Layers{};

    public:
        constexpr IExtensions() = default;
        virtual ~IExtensions() = default;

        /** Check if the layer with the given name is available in the system */
        [[nodiscard]] constexpr bool HasAvailableLayer(std::string_view const LayerName) const noexcept
        {
            auto const CompareLayerName = [&LayerName](Layer const& Iterator)
            {
                return std::equal(std::execution::unseq, std::cbegin(Iterator.Name), std::cend(Iterator.Name), std::cbegin(LayerName), std::cend(LayerName));
            };

            return std::find_if(std::execution::unseq, std::cbegin(m_Layers), std::cend(m_Layers), CompareLayerName) != std::cend(m_Layers);
        }

        /** Check if the extension with the given name is available in the system */
        [[nodiscard]] constexpr bool HasAvailableExtension(std::string_view const ExtensionName) const noexcept
        {
            auto const CompareExtensionName = [&ExtensionName](Pair<std::string, bool> const& Iterator)
            {
                return std::equal(std::execution::unseq, std::cbegin(Iterator.First), std::cend(Iterator.First), std::cbegin(ExtensionName), std::cend(ExtensionName));
            };

            auto const CheckLayerExtensions = [&CompareExtensionName](Layer const& Iterator)
            {
                return std::find_if(std::execution::unseq, std::cbegin(Iterator.Extensions), std::cend(Iterator.Extensions), CompareExtensionName) !=
                        std::cend(Iterator.Extensions);
            };

            return std::find_if(std::execution::unseq, std::cbegin(m_Layers), std::cend(m_Layers), CheckLayerExtensions) != std::cend(m_Layers);
        }

        /** Get the available layers */
        [[nodiscard]] std::vector<Layer> const& GetLayers() const
        {
            return m_Layers;
        }

        /** Set the state of the layer */
        void SetLayerState(std::string_view Layer, bool State, bool EnableAllExtensions = false);

        /** Get the available layers */
        void SetExtensionState(std::string_view FromLayer, std::string_view Extension, bool State);

        /** Get the enabled layers */
        [[nodiscard]] std::vector<const char*> GetEnabledLayersNames() const;

        /** Get the enabled extensions */
        [[nodiscard]] std::vector<const char*> GetEnabledExtensionsNames() const;

        /** Fill the extensions container */
        void FillExtensionsContainer();

    protected: /** Check for available extensions in the given layer */
        [[nodiscard]] virtual std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const = 0;

        /** Check for available layers and fill the layers container */
        [[nodiscard]] virtual std::vector<VkLayerProperties> FetchAvailableLayers() const = 0;
    };

    /** Object that will manage instance extensions */
    class LUVKMODULE_API InstanceExtensions : public IExtensions
    {
    public:
        constexpr InstanceExtensions() = default;
        ~InstanceExtensions() override = default;

    protected: /** Check for available extensions in the given layer */
        [[nodiscard]] std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const override;

        /** Check for available layers and fill the layers container */
        [[nodiscard]] std::vector<VkLayerProperties> FetchAvailableLayers() const override;
    };

    /** Object that will manage device extensions */
    class LUVKMODULE_API DeviceExtensions : public IExtensions
    {
        VkPhysicalDevice m_Device{VK_NULL_HANDLE};

    public:
        constexpr DeviceExtensions() = default;
        constexpr explicit DeviceExtensions(const VkPhysicalDevice Device) : m_Device(Device) {}
        ~DeviceExtensions() override = default;

        /** Update the physical device used for querying extensions */
        constexpr void SetDevice(const VkPhysicalDevice Device) noexcept
        {
            m_Device = Device;
        }

    protected: /** Check for available extensions in the given layer */
        [[nodiscard]] std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const override;

        /** Check for available layers and fill the layers container */
        [[nodiscard]] std::vector<VkLayerProperties> FetchAvailableLayers() const override;
    };
} // namespace luvk
