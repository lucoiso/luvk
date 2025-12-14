// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

namespace luvk
{
    class LUVKMODULE_API IFeatureChainModule
    {
    public:
        virtual ~IFeatureChainModule() = default;

        [[nodiscard]] virtual const void* GetDeviceFeatureChain() const
        {
            return nullptr;
        }

        [[nodiscard]] virtual const void* GetInstanceFeatureChain() const
        {
            return nullptr;
        }
    };
} // namespace luvk
