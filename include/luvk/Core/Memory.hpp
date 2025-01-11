// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Core/IRenderModule.hpp"

#include <vma/vk_mem_alloc.h>

namespace luvk
{
    /** Render module responsible for memory operations */
    class LUVKMODULE_API Memory : public IRenderModule
    {
        VmaAllocator m_Allocator {VK_NULL_HANDLE};

    public:
        constexpr Memory() = default;
        ~Memory() override = default;

        /** Initialize the allocator */
        void InitializeAllocator(std::shared_ptr<IRenderModule> const& MainRenderer,
                                 std::shared_ptr<IRenderModule> const& DeviceModule,
                                 VmaAllocatorCreateFlags Flags);

        /** Get the allocator */
        [[nodiscard]] inline VmaAllocator const& GetAllocator() const
        {
            return m_Allocator;
        }

    private:
        /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources(IRenderModule* MainRenderer) override;
    };
} // namespace luvk