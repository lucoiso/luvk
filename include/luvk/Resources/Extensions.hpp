// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <execution>
#include <string>
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

        [[nodiscard]] constexpr bool HasAvailableLayer(const std::string_view LayerName) const noexcept
        {
            const auto CompareLayerName = [&LayerName](const Layer& Iterator)
            {
                return std::equal(std::execution::unseq,
                                  std::cbegin(Iterator.Name),
                                  std::cend(Iterator.Name),
                                  std::cbegin(LayerName),
                                  std::cend(LayerName));
            };

            return std::find_if(std::execution::unseq, std::cbegin(m_Layers), std::cend(m_Layers), CompareLayerName) != std::cend(m_Layers);
        }

        [[nodiscard]] constexpr bool HasAvailableExtension(const std::string_view ExtensionName) const noexcept
        {
            const auto CompareExtensionName = [&ExtensionName](const std::pair<std::string, bool>& Iterator)
            {
                return std::equal(std::execution::unseq,
                                  std::cbegin(Iterator.first),
                                  std::cend(Iterator.first),
                                  std::cbegin(ExtensionName),
                                  std::cend(ExtensionName));
            };

            const auto CheckLayerExtensions = [&CompareExtensionName](const Layer& Iterator)
            {
                return std::find_if(std::execution::unseq,
                                    std::cbegin(Iterator.Extensions),
                                    std::cend(Iterator.Extensions),
                                    CompareExtensionName) != std::cend(Iterator.Extensions);
            };

            return std::find_if(std::execution::unseq, std::cbegin(m_Layers), std::cend(m_Layers), CheckLayerExtensions) != std::cend(m_Layers);
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
        constexpr explicit DeviceExtensions(VkPhysicalDevice Device) noexcept : m_Device(Device) {}
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
