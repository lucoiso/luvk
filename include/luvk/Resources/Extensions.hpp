// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <execution>
#include <string>
#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Libraries/CompileTime.hpp"
#include "luvk/Types/Layer.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    class LUVKMODULE_API IExtensions
    {
        Vector<Layer> m_Layers{};

    public:
        constexpr IExtensions() = default;
        virtual ~IExtensions() = default;

        [[nodiscard]] constexpr bool IsEmpty() const
        {
            return std::empty(m_Layers);
        }

        [[nodiscard]] constexpr bool HasAvailableLayer(const std::string_view LayerName) const noexcept
        {
            const auto CompareLayerName = [&LayerName](const Layer& Iterator)
            {
                return std::equal(std::execution::unseq, std::cbegin(Iterator.Name), std::cend(Iterator.Name), std::cbegin(LayerName), std::cend(LayerName));
            };

            return std::find_if(std::execution::unseq, std::cbegin(m_Layers), std::cend(m_Layers), CompareLayerName) != std::cend(m_Layers);
        }

        [[nodiscard]] constexpr bool HasAvailableExtension(const std::string_view ExtensionName) const noexcept
        {
            const auto CompareExtensionName = [&ExtensionName](const Pair<std::string, bool>& Iterator)
            {
                return std::equal(std::execution::unseq, std::cbegin(Iterator.First), std::cend(Iterator.First), std::cbegin(ExtensionName), std::cend(ExtensionName));
            };

            const auto CheckLayerExtensions = [&CompareExtensionName](const Layer& Iterator)
            {
                return std::find_if(std::execution::unseq, std::cbegin(Iterator.Extensions), std::cend(Iterator.Extensions), CompareExtensionName) !=
                        std::cend(Iterator.Extensions);
            };

            return std::find_if(std::execution::unseq, std::cbegin(m_Layers), std::cend(m_Layers), CheckLayerExtensions) != std::cend(m_Layers);
        }

        [[nodiscard]] const Vector<Layer>& GetLayers() const
        {
            return m_Layers;
        }

        void SetLayerState(std::string_view Layer, bool State, bool EnableAllExtensions = false);
        void SetExtensionState(std::string_view FromLayer, std::string_view Extension, bool State);

        [[nodiscard]] Vector<const char*> GetEnabledLayersNames() const;
        [[nodiscard]] Vector<const char*> GetEnabledExtensionsNames() const;

        void FillExtensionsContainer();

    protected:
        [[nodiscard]] virtual Vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const = 0;
        [[nodiscard]] virtual Vector<VkLayerProperties> FetchAvailableLayers() const = 0;
    };

    class LUVKMODULE_API InstanceExtensions : public IExtensions
    {
    public:
        constexpr InstanceExtensions() = default;
        ~InstanceExtensions() override = default;

    protected:
        [[nodiscard]] Vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const override;
        [[nodiscard]] Vector<VkLayerProperties> FetchAvailableLayers() const override;
    };

    class LUVKMODULE_API DeviceExtensions : public IExtensions
    {
        VkPhysicalDevice m_Device{VK_NULL_HANDLE};

    public:
        constexpr DeviceExtensions() = default;
        constexpr explicit DeviceExtensions(const VkPhysicalDevice& Device) : m_Device(Device) {}
        ~DeviceExtensions() override = default;

        constexpr void SetDevice(const VkPhysicalDevice& Device) noexcept
        {
            m_Device = Device;
        }

    protected:
        [[nodiscard]] Vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const override;
        [[nodiscard]] Vector<VkLayerProperties> FetchAvailableLayers() const override;
    };
} // namespace luvk
