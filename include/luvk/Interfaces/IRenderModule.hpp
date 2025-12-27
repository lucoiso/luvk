// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <memory>

namespace luvk
{
    class LUVK_API IRenderModule
    {
    public:
        virtual      ~IRenderModule() = default;
        virtual void InitializeResources() {}
        virtual void ClearResources() {}
    };

    using RenderModulePtr = std::shared_ptr<IRenderModule>;

    template <typename ModuleType, typename... Arguments>
    static std::shared_ptr<ModuleType> CreateModule(Arguments&&... Args)
    {
        return std::make_shared<ModuleType>(std::forward<Arguments>(Args)...);
    }
} // namespace luvk
