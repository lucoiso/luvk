// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <vma/vk_mem_alloc.h>
#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

namespace luvk
{
    enum class MemoryEvents : std::uint8_t
    {
        OnAllocatorCreated, OnAllocatorDestroyed
    };

    class LUVKMODULE_API Memory : public IRenderModule
    {
        VmaAllocator m_Allocator{VK_NULL_HANDLE};
        std::shared_ptr<IRenderModule> m_DeviceModule{};

    public:
        constexpr Memory() = default;

        ~Memory() override
        {
            Memory::ClearResources();
        }

        void InitializeAllocator(std::shared_ptr<IRenderModule> const& MainRenderer,
                                 std::shared_ptr<IRenderModule> const& DeviceModule,
                                 VmaAllocatorCreateFlags Flags);

        [[nodiscard]] VmaAllocator const& GetAllocator() const
        {
            return m_Allocator;
        }

        [[nodiscard]] std::shared_ptr<IRenderModule> const& GetDeviceModule() const
        {
            return m_DeviceModule;
        }

        void QueryMemoryStats(bool AbortOnCritical = false) const;

    private:
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override;
        void ClearResources() override;
    };
} // namespace luvk
