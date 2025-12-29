/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <volk.h>

namespace luvk
{
    /** Represents a Vulkan validation layer with its extensions */
    struct LUVK_API Layer
    {
        /** Whether the layer is currently enabled for creation. */
        bool Enabled{};

        /** The official name of the Vulkan layer. */
        std::string Name{};

        /** List of extensions supported by this layer (Extension Name, Enabled State). */
        std::vector<std::pair<std::string, bool>> Extensions;
    };

    /** Base interface for managing Vulkan layers and extensions */
    class LUVK_API IExtensions
    {
        /** The container holding all available layers and their extensions. */
        std::vector<Layer> m_Layers{};

    public:
        /** Default constructor */
        constexpr IExtensions() = default;

        /** Virtual destructor */
        virtual ~IExtensions() = default;

        /** Check if layer list is empty */
        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return std::empty(m_Layers);
        }

        /** Check if a specific layer is available */
        [[nodiscard]] constexpr bool HasAvailableLayer(std::string_view LayerName) const noexcept
        {
            return std::ranges::any_of(m_Layers,
                                       [&LayerName](const Layer& Iterator)
                                       {
                                           return std::ranges::equal(Iterator.Name, LayerName);
                                       });
        }

        /** Check if a specific extension is available */
        [[nodiscard]] constexpr bool HasAvailableExtension(std::string_view ExtensionName) const noexcept
        {
            return std::ranges::any_of(m_Layers,
                                       [&ExtensionName](const Layer& Iterator)
                                       {
                                           return std::ranges::any_of(Iterator.Extensions,
                                                                      [&ExtensionName](const std::pair<std::string, bool>& ExtIterator)
                                                                      {
                                                                          return std::ranges::equal(ExtIterator.first, ExtensionName);
                                                                      });
                                       });
        }

        /** Get all available layers */
        [[nodiscard]] const std::vector<Layer>& GetLayers() const noexcept
        {
            return m_Layers;
        }

        /** Enable or disable a layer and optionally all its extensions */
        bool SetLayerState(std::string_view Layer, bool State, bool EnableAllExtensions = false);

        /** Enable or disable a specific extension in a layer */
        bool SetExtensionState(std::string_view FromLayer, std::string_view Extension, bool State);

        /** Get names of all enabled layers */
        [[nodiscard]] std::vector<const char*> GetEnabledLayersNames() const;

        /** Get names of all enabled extensions */
        [[nodiscard]] std::vector<const char*> GetEnabledExtensionsNames() const;

        /** Query and populate available layers and extensions */
        void FillExtensionsContainer();

    protected:
        /** Query available extensions for a specific layer */
        [[nodiscard]] virtual std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const = 0;

        /** Query all available layers */
        [[nodiscard]] virtual std::vector<VkLayerProperties> FetchAvailableLayers() const = 0;
    };

    /** Manager for instance-level extensions and layers */
    class LUVK_API InstanceExtensions : public IExtensions
    {
    public:
        /** Default constructor */
        constexpr InstanceExtensions() = default;

        /** Destructor */
        ~InstanceExtensions() override = default;

    protected:
        /** Query available extensions for a specific layer at the instance level. */
        [[nodiscard]] std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const override;
        /** Query all available layers at the instance level. */
        [[nodiscard]] std::vector<VkLayerProperties> FetchAvailableLayers() const override;
    };

    /** Manager for device-level extensions and layers */
    class LUVK_API DeviceExtensions : public IExtensions
    {
        /** The physical device used to query device-specific extensions. */
        VkPhysicalDevice m_Device{VK_NULL_HANDLE};

    public:
        /** Default constructor */
        constexpr DeviceExtensions() = default;

        /** Constructor with physical device */
        constexpr explicit DeviceExtensions(const VkPhysicalDevice Device) noexcept
            : m_Device(Device) {}

        /** Destructor */
        ~DeviceExtensions() override = default;

        /** Set the physical device to query extensions for */
        constexpr void SetDevice(const VkPhysicalDevice Device) noexcept
        {
            m_Device = Device;
        }

    protected:
        /** Query available extensions for a specific layer on the current physical device. */
        [[nodiscard]] std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const override;
        /** Query all available layers on the current physical device. */
        [[nodiscard]] std::vector<VkLayerProperties> FetchAvailableLayers() const override;
    };
}
