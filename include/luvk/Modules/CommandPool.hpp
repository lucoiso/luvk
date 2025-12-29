/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <span>
#include <vector>
#include <volk.h>
#include "luvk/Interfaces/IModule.hpp"

namespace luvk
{
    /**
     * Module managing a main Vulkan Command Pool for per-frame command buffers.
     */
    class LUVK_API CommandPool : public IModule
    {
        /** The Vulkan command pool handle. */
        VkCommandPool m_Pool{VK_NULL_HANDLE};

        /** A list of allocated command buffers (not strictly managed here, but can be a cache). */
        std::vector<VkCommandBuffer> m_Buffers{};

        /** Pointer to the central service locator. */
        IServiceLocator* m_ServiceLocator{nullptr};

    public:
        /** Default destructor. */
        ~CommandPool() override = default;

        /** Called upon module initialization (creates the command pool). */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /** Called upon module shutdown (destroys the command pool). */
        void OnShutdown() override;

        /**
         * Allocates new command buffers from the pool.
         * @param Count The number of command buffers to allocate.
         * @param Level The command buffer level (Primary or Secondary).
         * @return A vector of the newly allocated VkCommandBuffers.
         */
        std::vector<VkCommandBuffer> Allocate(std::uint32_t Count, VkCommandBufferLevel Level = VK_COMMAND_BUFFER_LEVEL_PRIMARY);

        /**
         * Frees allocated command buffers back to the pool.
         * @param Buffers Span of command buffers to free.
         */
        void Free(std::span<const VkCommandBuffer> Buffers) const;

        /**
         * Resets the command pool, making all command buffers available for reuse.
         * @param ReleaseResources If true, the pool may free all of its memory.
         */
        void Reset(bool ReleaseResources = false) const;

        /** Get the underlying VkCommandPool handle. */
        [[nodiscard]] constexpr VkCommandPool GetHandle() const noexcept
        {
            return m_Pool;
        }
    };
}
