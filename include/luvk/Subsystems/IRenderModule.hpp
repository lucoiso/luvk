// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Modules/Module.hpp"
#include "luvk/Subsystems/Event.hpp"
#include <memory>
#include <vector>
#include <string_view>

namespace luvk
{
    /** Utility to map a key to a list of extensions */
    template <typename KeyType, typename ValueType>
    std::unordered_map<std::string_view, std::vector<std::string_view>> ToExtensionMap(KeyType&& Key, std::initializer_list<ValueType>&& Values)
    {
        std::vector<std::string_view> NewEntries;
        NewEntries.reserve(std::size(Values));

        for (const auto& ValueIt : Values)
        {
            NewEntries.push_back(std::string_view{ValueIt});
        }

        return std::unordered_map<std::string_view, std::vector<std::string_view>>{{std::string_view{Key}, NewEntries}};
    }

    /** Interface for a render module */
    class LUVKMODULE_API IRenderModule
    {
        /** Graph storing registered events */
        EventGraph m_EventGraph{};

    public:
        constexpr IRenderModule() = default;
        virtual ~IRenderModule() = default;

        /** Get the event management system */
        [[nodiscard]] EventGraph& GetEventSystem()
        {
            return m_EventGraph;
        }

        /** Initialize the dependencies of this module */
        virtual void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) = 0;

        /** Clear the resources of this module */
        virtual void ClearResources() = 0;

        /** List required instance extensions */
        [[nodiscard]] virtual std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredInstanceExtensions() const
        {
            return {};
        }

        /** List required device extensions */
        [[nodiscard]] virtual std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredDeviceExtensions() const
        {
            return {};
        }

        /** Optional chain of device features
         *  \warning The returned pointer must remain valid until the logical
         *  device creation finishes.
         */
        [[nodiscard]] virtual void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const& DeviceModule) const noexcept = 0;

        /** Optional chain of instance features
         *  \warning The returned pointer must remain valid until the instance
         *  creation finishes.
         */
        [[nodiscard]] virtual void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept = 0;
    };
} // namespace luvk
