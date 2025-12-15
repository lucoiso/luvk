// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include <span>
#include <volk.h>
#include "luvk/Module.hpp"

namespace luvk
{
    class Device;
    class DescriptorPool;
    class Memory;

    class LUVKMODULE_API DescriptorSet
    {
        bool                            m_OwnsLayout{false};
        VkDescriptorSetLayout           m_Layout{VK_NULL_HANDLE};
        VkDescriptorSet                 m_Set{VK_NULL_HANDLE};
        std::shared_ptr<Device>         m_DeviceModule{};
        std::shared_ptr<DescriptorPool> m_PoolModule{};
        std::shared_ptr<Memory>         m_MemoryModule{};

    public:
        DescriptorSet() = delete;
        explicit DescriptorSet(const std::shared_ptr<Device>&         DeviceModule,
                               const std::shared_ptr<DescriptorPool>& PoolModule,
                               const std::shared_ptr<Memory>&         MemoryModule);

        ~DescriptorSet();

        struct LayoutInfo
        {
            std::span<const VkDescriptorSetLayoutBinding> Bindings{};
        };

        void CreateLayout(const LayoutInfo& Info);
        void UseLayout(const VkDescriptorSetLayout& Layout);
        void Allocate();

        void UpdateBuffer(const VkBuffer&  Buffer,
                          VkDeviceSize     Size,
                          std::uint32_t    Binding,
                          VkDescriptorType Type) const;

        void UpdateImage(const VkImageView& View,
                         const VkSampler&   Sampler,
                         std::uint32_t      Binding,
                         VkDescriptorType   Type) const;

        [[nodiscard]] constexpr const VkDescriptorSetLayout& GetLayout() const
        {
            return m_Layout;
        }

        [[nodiscard]] constexpr const VkDescriptorSet& GetHandle() const
        {
            return m_Set;
        }
    };
} // namespace luvk
