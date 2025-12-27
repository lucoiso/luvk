// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <vk_mem_alloc.h>
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Renderer;
    class Device;

    class LUVK_API Memory : public IRenderModule
    {
    protected:
        VmaAllocator            m_Allocator{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};
        std::weak_ptr<Renderer> m_RendererModule{};

    public:
        Memory() = delete;
        explicit Memory(const std::shared_ptr<Renderer>& RendererModule, const std::shared_ptr<Device>& DeviceModule);

        ~Memory() override
        {
            Memory::ClearResources();
        }

        void InitializeAllocator(VmaAllocatorCreateFlags Flags);

        [[nodiscard]] constexpr VmaAllocator GetAllocator() const noexcept
        {
            return m_Allocator;
        }

    protected:
        void ClearResources() override;
    };
} // namespace luvk
