/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

namespace luvk
{
    /**
     * Interface for modules that need to append features to the Vulkan pNext chain.
     */
    class LUVK_API IFeatureChainModule
    {
    public:
        /** Virtual destructor. */
        virtual ~IFeatureChainModule() = default;

        /**
         * Get the feature structure to append to Device creation pNext chain.
         * @return Pointer to a Vulkan structure (VkBaseOutStructure*).
         */
        [[nodiscard]] virtual const void* GetDeviceFeatureChain() const noexcept
        {
            return nullptr;
        }

        /**
         * Get the feature structure to append to Instance creation pNext chain.
         * @return Pointer to a Vulkan structure (VkBaseInStructure*).
         */
        [[nodiscard]] virtual const void* GetInstanceFeatureChain() const noexcept
        {
            return nullptr;
        }
    };
}
