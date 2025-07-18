// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <span>
#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Subsystems/IRenderModule.hpp"

namespace luvk
{
    class LUVKMODULE_API DescriptorPool : public IRenderModule
    {
        VkDescriptorPool m_Pool{VK_NULL_HANDLE};
        std::shared_ptr<IRenderModule> m_DeviceModule{};

    public:
        constexpr DescriptorPool() = default;

        ~DescriptorPool() override
        {
            DescriptorPool::ClearResources();
        }

        void CreateDescriptorPool(std::shared_ptr<IRenderModule> const& DeviceModule,
                                  std::uint32_t MaxSets,
                                  std::span<VkDescriptorPoolSize const> PoolSizes,
                                  VkDescriptorPoolCreateFlags Flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

        [[nodiscard]] constexpr VkDescriptorPool const& GetHandle() const
        {
            return m_Pool;
        }

    private:
        void InitializeDependencies(std::shared_ptr<IRenderModule> const&) override;
        void ClearResources() override;
    };
} // namespace luvk
