// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Types/Layer.hpp"
#include "luvk/Libraries/Compile.hpp"

#include <execution>
#include <string>
#include <vector>
#include <Volk/volk.h>

namespace luvk
{
    constexpr std::size_t LayersCapacity = 256U;
    using LayerArray = Array<Layer, LayersCapacity>;
    using ExtensionNameArray = Array<const char*, LayersCapacity>;

    class LUVKMODULE_API Renderer
    {
        VkInstance m_Instance {};
        LayerArray m_Layers {};

    public:
        Renderer();
        ~Renderer() = default;

        /* Check if the layer with the given name is available in the system */
        [[nodiscard]] constexpr bool HasAvailableLayer(std::string_view const LayerName) const noexcept
        {
            auto const CompareLayerName = [&LayerName] (Layer const& Iterator)
            {
                return std::equal(std::execution::unseq,
                                  std::begin(Iterator.Name),
                                  std::end(Iterator.Name),
                                  std::begin(LayerName),
                                  std::end(LayerName));
            };

            return std::find_if(std::execution::unseq,
                                std::begin(m_Layers),
                                std::end(m_Layers),
                                CompareLayerName) != std::end(m_Layers);
        }

        /* Check if the extension with the given name is available in the system */
        [[nodiscard]] constexpr bool HasAvailableExtension(std::string_view const ExtensionName) const noexcept
        {
            auto const CompareExtensionName = [&ExtensionName] (Pair<std::string, bool> const& Iterator)
            {
                return std::equal(std::execution::unseq,
                                  std::begin(Iterator.First),
                                  std::end(Iterator.First),
                                  std::begin(ExtensionName),
                                  std::end(ExtensionName));
            };

            auto const CheckLayerExtensions = [&CompareExtensionName] (Layer const& Iterator)
            {
                return std::find_if(std::execution::unseq,
                                    std::begin(Iterator.Extensions),
                                    std::end(Iterator.Extensions),
                                    CompareExtensionName) != std::end(Iterator.Extensions);
            };

            return std::find_if(std::execution::unseq,
                                std::begin(m_Layers),
                                std::end(m_Layers),
                                CheckLayerExtensions) != std::end(m_Layers);
        }

        /* Get the available layers */
        [[nodiscard]] inline luvk::LayerArray const& GetLayers() const
        {
            return m_Layers;
        }

        /* Get the available layers */
        [[nodiscard]] inline luvk::LayerArray& GetMutableLayers()
        {
            return m_Layers;
        }

        /* Get the enabled layers */
        [[nodiscard]] luvk::ExtensionNameArray GetEnabledLayersNames() const;

        /* Get the enabled extensions */
        [[nodiscard]] luvk::ExtensionNameArray GetEnabledExtensionsNames() const;

        /* Arguments to create the vulkan instance */
        struct InstanceCreationArguments
        {
            std::string_view ApplicationName;
            std::string_view EngineName;
            std::uint32_t ApplicationVersion;
            std::uint32_t EngineVersion;
        };

        /** Initialize instance resources */
        [[nodiscard]] bool InitializeRenderer(InstanceCreationArguments const& Arguments);

    private:
        /** Initialize instance dependencies */
        void InitializeDependencies();

        /* Check for available extensions in the given layer */
        static std::vector<Pair<std::string, bool>> FetchAvailableLayerExtensions(std::string_view LayerName);

        /* Check for available layers and fill the layers container */
        void FetchAvailableLayers();
    };
} // namespace luvk