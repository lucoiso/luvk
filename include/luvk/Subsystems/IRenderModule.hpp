// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <string_view>
#include "luvk/Module.hpp"
#include "luvk/Resources/Event.hpp"
#include "luvk/Types/UnorderedMap.hpp"
#include "luvk/Types/Vector.hpp"

namespace luvk
{
    template <typename KeyType, typename ValueType>
    UnorderedMap<std::string_view, Vector<std::string_view>> ToExtensionMap(KeyType&& Key, std::initializer_list<ValueType>&& Values)
    {
        Vector<std::string_view> NewEntries;
        NewEntries.reserve(std::size(Values));

        for (const auto& ValueIt : Values)
        {
            NewEntries.push_back(std::string_view{ValueIt});
        }

        return luvk::UnorderedMap<std::string_view, Vector<std::string_view>>{{std::string_view{Key}, NewEntries}};
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

        virtual void InitializeDependencies(const std::shared_ptr<IRenderModule>& MainRenderer) = 0;
        virtual void ClearResources() = 0;

        [[nodiscard]] virtual UnorderedMap<std::string_view, Vector<std::string_view>> GetRequiredInstanceExtensions() const
        {
            return {};
        }

        [[nodiscard]] virtual UnorderedMap<std::string_view, Vector<std::string_view>> GetRequiredDeviceExtensions() const
        {
            return {};
        }

        [[nodiscard]] virtual const void* GetDeviceFeatureChain(const std::shared_ptr<IRenderModule>& DeviceModule) const noexcept
        {
            return nullptr;
        }

        [[nodiscard]] virtual const void* GetInstanceFeatureChain(const std::shared_ptr<IRenderModule>& RendererModule) const noexcept
        {
            return nullptr;
        }
    };
} // namespace luvk
