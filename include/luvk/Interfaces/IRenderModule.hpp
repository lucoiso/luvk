// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include "luvk/Module.hpp"

namespace luvk
{
    class LUVKMODULE_API IRenderModule
    {
    public:
        virtual      ~IRenderModule() = default;
        virtual void InitializeResources() {}
        virtual void ClearResources() {}
    };

    using RenderModulePtr = std::shared_ptr<luvk::IRenderModule>;

    template <typename ModuleType, typename... Arguments>
    static std::shared_ptr<ModuleType> CreateModule(Arguments&&... Args)
    {
        return std::make_shared<ModuleType>(std::forward<Arguments>(Args)...);
    }
} // namespace luvk
