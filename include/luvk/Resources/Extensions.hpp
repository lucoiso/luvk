// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <algorithm>
#include <string>
#include <utility>
#include <vector>
#include <volk.h>

namespace luvk
{
    struct LUVK_API Layer
    {
        bool                                      Enabled{};
        std::string                               Name{};
        std::vector<std::pair<std::string, bool>> Extensions;
    };

    class LUVK_API IExtensions
    {
        std::vector<Layer> m_Layers{};

    public:
        constexpr IExtensions()  = default;
        virtual   ~IExtensions() = default;

        [[nodiscard]] constexpr bool IsEmpty() const noexcept
        {
            return std::empty(m_Layers);
        }

        [[nodiscard]] constexpr bool HasAvailableLayer(std::string_view LayerName) const noexcept
        {
            return std::ranges::any_of(m_Layers,
                                       [&LayerName](const Layer& Iterator)
                                       {
                                           return std::ranges::equal(Iterator.Name, LayerName);
                                       });
        }

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

        [[nodiscard]] const std::vector<Layer>& GetLayers() const noexcept
        {
            return m_Layers;
        }

        bool SetLayerState(std::string_view Layer, bool State, bool EnableAllExtensions = false);
        bool SetExtensionState(std::string_view FromLayer, std::string_view Extension, bool State);

        [[nodiscard]] std::vector<const char*> GetEnabledLayersNames() const;
        [[nodiscard]] std::vector<const char*> GetEnabledExtensionsNames() const;

        void FillExtensionsContainer();

    protected:
        [[nodiscard]] virtual std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const = 0;
        [[nodiscard]] virtual std::vector<VkLayerProperties>     FetchAvailableLayers() const = 0;
    };

    class LUVK_API InstanceExtensions : public IExtensions
    {
    public:
        constexpr InstanceExtensions() = default;
        ~InstanceExtensions() override = default;

    protected:
        [[nodiscard]] std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const override;
        [[nodiscard]] std::vector<VkLayerProperties>     FetchAvailableLayers() const override;
    };

    class LUVK_API DeviceExtensions : public IExtensions
    {
        VkPhysicalDevice m_Device{VK_NULL_HANDLE};

    public:
        constexpr          DeviceExtensions() = default;
        constexpr explicit DeviceExtensions(const VkPhysicalDevice Device) noexcept : m_Device(Device) {}
        ~DeviceExtensions() override = default;

        constexpr void SetDevice(VkPhysicalDevice Device) noexcept
        {
            m_Device = Device;
        }

    protected:
        [[nodiscard]] std::vector<VkExtensionProperties> FetchAvailableLayerExtensions(std::string_view LayerName) const override;
        [[nodiscard]] std::vector<VkLayerProperties>     FetchAvailableLayers() const override;
    };
} // namespace luvk
