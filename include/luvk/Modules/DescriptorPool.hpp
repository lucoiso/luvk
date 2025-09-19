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
    class Device;

    class LUVKMODULE_API DescriptorPool : public IRenderModule
    {
        VkDescriptorPool m_Pool{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        constexpr DescriptorPool() = default;

        ~DescriptorPool() override
        {
            DescriptorPool::ClearResources();
        }

        void CreateDescriptorPool(const std::shared_ptr<Device>& DeviceModule,
                                  std::uint32_t MaxSets,
                                  const std::span<const VkDescriptorPoolSize>& PoolSizes,
                                  VkDescriptorPoolCreateFlags Flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

        [[nodiscard]] constexpr const VkDescriptorPool& GetHandle() const
        {
            return m_Pool;
        }

    private:
        void InitializeDependencies(const std::shared_ptr<IRenderModule>&) override;
        void ClearResources() override;
    };
} // namespace luvk
