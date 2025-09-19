// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <volk/volk.h>
#include "luvk/Module.hpp"

namespace luvk
{
    class Device;
    class DescriptorPool;
    class Memory;

    class LUVKMODULE_API DescriptorSet
    {
        bool m_OwnsLayout{false};
        VkDescriptorSetLayout m_Layout{VK_NULL_HANDLE};
        VkDescriptorSet m_Set{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};
        std::shared_ptr<DescriptorPool> m_PoolModule{};
        std::shared_ptr<Memory> m_MemoryModule{};

    public:
        constexpr DescriptorSet() = default;
        ~DescriptorSet();

        struct LayoutInfo
        {
            std::span<const VkDescriptorSetLayoutBinding> Bindings{};
        };

        void CreateLayout(const std::shared_ptr<Device>& DeviceModule,
                          const LayoutInfo& Info);

        void UseLayout(const std::shared_ptr<Device>& DeviceModule,
                       const VkDescriptorSetLayout& Layout);

        void Allocate(const std::shared_ptr<DescriptorPool>& PoolModule,
                      const std::shared_ptr<Memory>& MemoryModule = nullptr);

        void UpdateBuffer(const VkBuffer& Buffer,
                          VkDeviceSize Size,
                          std::uint32_t Binding,
                          VkDescriptorType Type) const;

        void UpdateImage(const VkImageView& View,
                         const VkSampler& Sampler,
                         std::uint32_t Binding,
                         VkDescriptorType Type) const;

        [[nodiscard]] constexpr const VkDescriptorSetLayout& GetLayout() const
        {
            return m_Layout;
        }

        [[nodiscard]] constexpr const VkDescriptorSet& GetHandle() const
        {
            return m_Set;
        }

    private:
        void Destroy();
    };
} // namespace luvk
