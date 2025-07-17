// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

#include <memory>
#include <vector>
#include <string_view>
#include <vma/vk_mem_alloc.h>
#include "luvk/Modules/Device.hpp"

namespace luvk
{
    /** Memory Event Keys */
    enum class MemoryEvents : std::uint8_t
    {
        OnAllocatorCreated, OnAllocatorDestroyed
    };

    /** Render module responsible for memory operations */
    class LUVKMODULE_API Memory : public IRenderModule
    {
        /** Handle to the Vulkan memory allocator */
        VmaAllocator m_Allocator{VK_NULL_HANDLE};

        /** Device module used by the allocator */
        std::shared_ptr<IRenderModule> m_DeviceModule{};

    public:
        constexpr Memory() = default;

        //~ Begin of IRenderModule interface
        /** Destructor releases the allocator */
        ~Memory() override
        {
            Memory::ClearResources();
        }

        /** Initialize the allocator */
        void InitializeAllocator(std::shared_ptr<IRenderModule> const& MainRenderer, std::shared_ptr<IRenderModule> const& DeviceModule, VmaAllocatorCreateFlags Flags);

        /** Get the allocator */
        [[nodiscard]] VmaAllocator const& GetAllocator() const
        {
            return m_Allocator;
        }

        [[nodiscard]] std::shared_ptr<IRenderModule> const& GetDeviceModule() const
        {
            return m_DeviceModule;
        }

        /** Query memory usage and log stats. Optionally abort if usage is critical */
        void QueryMemoryStats(bool AbortOnCritical = false) const;

        [[nodiscard]] std::unordered_map<std::string_view, std::vector<std::string_view>> GetRequiredDeviceExtensions() const override
        {
            return {};
        }

        [[nodiscard]] void const* GetDeviceFeatureChain(std::shared_ptr<IRenderModule> const&) const noexcept override
        {
            return nullptr;
        }

        [[nodiscard]] void const* GetInstanceFeatureChain(std::shared_ptr<IRenderModule> const& RendererModule) const noexcept override
        {
            return nullptr;
        }

    private: /** Initialize the dependencies of this module */
        void InitializeDependencies(std::shared_ptr<IRenderModule> const& MainRenderer) override;

        /** Clear the resources of this module */
        void ClearResources() override;
        //~ End of IRenderModule interface
    };
} // namespace luvk
