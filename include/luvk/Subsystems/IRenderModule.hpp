// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <string_view>
#include <vector>
#include "luvk/Module.hpp"
#include "luvk/Resources/Event.hpp"

namespace luvk
{
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

    class LUVKMODULE_API IRenderModule
    {
        EventGraph m_EventGraph{};

    public:
        constexpr IRenderModule() = default;
        virtual ~IRenderModule() = default;

        [[nodiscard]] EventGraph& GetEventSystem()
        {
            return m_EventGraph;
        }

        virtual void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) = 0;
        virtual void ClearResources() = 0;

        [[nodiscard]] virtual std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredInstanceExtensions() const
        {
            return {};
        }

        [[nodiscard]] virtual std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredDeviceExtensions() const
        {
            return {};
        }

        [[nodiscard]] virtual void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const& DeviceModule) const noexcept
        {
            return nullptr;
        }

        [[nodiscard]] virtual void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept
        {
            return nullptr;
        }
    };
} // namespace luvk
