/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <mutex>
#include <vector>
#include <volk.h>
#include "luvk/Interfaces/IModule.hpp"

namespace luvk
{
    /**
     * Module managing descriptor set allocation using a pool-per-frame strategy.
     */
    class LUVK_API DescriptorPool : public IModule
    {
        /** The currently active descriptor pool for allocations. */
        VkDescriptorPool m_CurrentPool{VK_NULL_HANDLE};

        /** Pools that were used in the current frame and will be recycled. */
        std::vector<VkDescriptorPool> m_UsedPools{};

        /** Pools that are free and can be reused. */
        std::vector<VkDescriptorPool> m_FreePools{};

        /** Mutex to synchronize access to the pools. */
        std::mutex m_Mutex{};

        /** Pointer to the central service locator. */
        IServiceLocator* m_ServiceLocator{nullptr};

    public:
        /** Default destructor. */
        ~DescriptorPool() override = default;

        /** Called upon module initialization. */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /** Called upon module shutdown (destroys all pools). */
        void OnShutdown() override;

        /**
         * Allocates a descriptor set from the current pool. Switches pool if necessary.
         * @param Layout The descriptor set layout to use for allocation.
         * @param Set Output VkDescriptorSet handle.
         * @return True if allocation was successful, false otherwise.
         */
        bool AllocateSet(VkDescriptorSetLayout Layout, VkDescriptorSet& Set);

        /**
         * Resets all used pools and prepares them for the next frame.
         */
        void Reset();

    private:
        /**
         * Retrieves a descriptor pool, either from the free list or by creating a new one.
         * @return The descriptor pool handle.
         */
        [[nodiscard]] VkDescriptorPool GetPool();

        /**
         * Creates a new descriptor pool with a predefined size and types.
         * @return The newly created descriptor pool handle.
         */
        [[nodiscard]] VkDescriptorPool CreatePool() const;
    };
}
