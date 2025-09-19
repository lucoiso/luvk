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
    class Renderer;
    class Device;

    enum class MemoryEvents : std::uint8_t
    {
        OnAllocatorCreated, OnAllocatorDestroyed
    };

    class LUVKMODULE_API Memory : public IRenderModule
    {
        VmaAllocator m_Allocator{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        constexpr Memory() = default;

        ~Memory() override
        {
            Memory::ClearResources();
        }

        void InitializeAllocator(const std::shared_ptr<Device>& DeviceModule,
                                 const VkInstance& Instance,
                                 VmaAllocatorCreateFlags Flags);

        [[nodiscard]] const VmaAllocator& GetAllocator() const
        {
            return m_Allocator;
        }

        [[nodiscard]] const std::shared_ptr<Device>& GetDeviceModule() const
        {
            return m_DeviceModule;
        }

        void QueryMemoryStats(bool AbortOnCritical = false) const;

    private:
        void InitializeDependencies(const std::shared_ptr<IRenderModule>&) override;
        void ClearResources() override;
    };
} // namespace luvk
