// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <vma/vk_mem_alloc.h>
#include "luvk/Module.hpp"
#include "luvk/Interfaces/IEventModule.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Renderer;
    class Device;

    enum class MemoryEvents : std::uint8_t
    {
        OnAllocatorCreated,
        OnAllocatorDestroyed
    };

    class LUVKMODULE_API Memory : public IRenderModule,
                                  public IEventModule
    {
        VmaAllocator m_Allocator{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};
        std::shared_ptr<Renderer> m_RendererModule{};

    public:
        Memory() = delete;
        explicit Memory(const std::shared_ptr<Renderer>& RendererModule, const std::shared_ptr<Device>& DeviceModule);

        ~Memory() override
        {
            Memory::ClearResources();
        }

        void InitializeAllocator(VmaAllocatorCreateFlags Flags);

        [[nodiscard]] const VmaAllocator& GetAllocator() const
        {
            return m_Allocator;
        }

    protected:
        void ClearResources() override;
    };
} // namespace luvk
