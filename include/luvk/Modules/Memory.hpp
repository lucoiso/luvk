/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <vk_mem_alloc.h>
#include "luvk/Interfaces/IModule.hpp"

namespace luvk
{
    /**
     * Module managing the Vulkan Memory Allocator (VMA) library.
     */
    class LUVK_API Memory : public IModule
    {
        /** The VMA allocator handle. */
        VmaAllocator m_Allocator{VK_NULL_HANDLE};

        /** Pointer to the central service locator. */
        IServiceLocator* m_ServiceLocator{nullptr};

    public:
        /** Default destructor. */
        ~Memory() override = default;

        /** Called upon module initialization (creates the VMA allocator). */
        void OnInitialize(IServiceLocator* ServiceLocator) override;

        /** Called upon module shutdown (destroys the VMA allocator). */
        void OnShutdown() override;

        /** Get the VMA allocator handle. */
        [[nodiscard]] constexpr VmaAllocator GetAllocator() const noexcept
        {
            return m_Allocator;
        }
    };
}
