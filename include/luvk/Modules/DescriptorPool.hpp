// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <span>
#include <volk/volk.h>
#include "luvk/Module.hpp"
#include "luvk/Interfaces/IRenderModule.hpp"

namespace luvk
{
    class Device;

    class LUVKMODULE_API DescriptorPool : public IRenderModule
    {
        VkDescriptorPool m_Pool{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};

    public:
        DescriptorPool() = delete;
        explicit DescriptorPool(const std::shared_ptr<Device>& DeviceModule);

        ~DescriptorPool() override
        {
            DescriptorPool::ClearResources();
        }

        void CreateDescriptorPool(std::uint32_t MaxSets,
                                  const std::span<const VkDescriptorPoolSize>& PoolSizes,
                                  VkDescriptorPoolCreateFlags Flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

        [[nodiscard]] constexpr const VkDescriptorPool& GetHandle() const
        {
            return m_Pool;
        }

    protected:
        void ClearResources() override;
    };
} // namespace luvk
