// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#pragma once

namespace luvk
{
    class LUVK_API IFeatureChainModule
    {
    public:
        virtual ~IFeatureChainModule() = default;

        [[nodiscard]] virtual const void* GetDeviceFeatureChain() const noexcept
        {
            return nullptr;
        }

        [[nodiscard]] virtual const void* GetInstanceFeatureChain() const noexcept
        {
            return nullptr;
        }
    };
} // namespace luvk
