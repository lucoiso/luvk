// Author: Lucas Vilas-Boas
// Year : 2024
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

namespace luvk
{
    /** Interface for a render module */
    class LUVKMODULE_API IRenderModule
    {
    public:
        constexpr IRenderModule() = default;
        virtual ~IRenderModule() = default;

        /** Initialize the dependencies of this module */
        virtual void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) = 0;
    };
} // namespace luvk