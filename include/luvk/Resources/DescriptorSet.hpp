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
        VkDescriptorSetLayout m_Layout{VK_NULL_HANDLE};
        VkDescriptorSet m_Set{VK_NULL_HANDLE};
        std::shared_ptr<Device> m_DeviceModule{};
        std::shared_ptr<DescriptorPool> m_PoolModule{};
        std::shared_ptr<Memory> m_MemoryModule{};
        bool m_OwnsLayout{false};

    public:
        constexpr DescriptorSet() = default;
        ~DescriptorSet();

        struct LayoutInfo
        {
            std::span<VkDescriptorSetLayoutBinding const> Bindings{};
        };

        void CreateLayout(std::shared_ptr<Device> const& DeviceModule, LayoutInfo const& Info);
        void UseLayout(std::shared_ptr<Device> const& DeviceModule, VkDescriptorSetLayout Layout);

        void Allocate(std::shared_ptr<Device> const& DeviceModule,
                      std::shared_ptr<DescriptorPool> const& PoolModule,
                      std::shared_ptr<Memory> const& MemoryModule = nullptr);

        void UpdateBuffer(std::shared_ptr<Device> const& DeviceModule,
                          VkBuffer Buffer,
                          VkDeviceSize Size,
                          std::uint32_t Binding,
                          VkDescriptorType Type) const;

        void UpdateImage(std::shared_ptr<Device> const& DeviceModule,
                         VkImageView View,
                         VkSampler Sampler,
                         std::uint32_t Binding,
                         VkDescriptorType Type) const;

        [[nodiscard]] constexpr VkDescriptorSetLayout const& GetLayout() const
        {
            return m_Layout;
        }

        [[nodiscard]] constexpr VkDescriptorSet const& GetHandle() const
        {
            return m_Set;
        }

    private:
        void Destroy();
    };
} // namespace luvk
