/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <memory>
#include <span>
#include <volk.h>

namespace luvk
{
    class Device;
    class DescriptorPool;
    class Memory;

    class LUVK_API DescriptorSet
    {
    protected:
        bool                            m_OwnsLayout{false};
        bool                            m_Owned{true};
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

        explicit DescriptorSet(const std::shared_ptr<Device>&         DeviceModule,
                               const std::shared_ptr<DescriptorPool>& PoolModule,
                               const std::shared_ptr<Memory>&         MemoryModule,
                               VkDescriptorSet                        ExistingSet,
                               VkDescriptorSetLayout                  ExistingLayout = VK_NULL_HANDLE);

        ~DescriptorSet();

        struct LayoutInfo
        {
            std::span<const VkDescriptorSetLayoutBinding> Bindings{};
        };

        void CreateLayout(const LayoutInfo& Info);
        void UseLayout(VkDescriptorSetLayout Layout);
        void Allocate();

        void UpdateBuffer(VkBuffer Buffer, VkDeviceSize Size, std::uint32_t Binding, VkDescriptorType Type) const;

        void UpdateImage(VkImageView View, VkSampler Sampler, std::uint32_t Binding, VkDescriptorType Type) const;

        [[nodiscard]] constexpr VkDescriptorSetLayout GetLayout() const noexcept
        {
            return m_Layout;
        }

        [[nodiscard]] constexpr VkDescriptorSet GetHandle() const noexcept
        {
            return m_Set;
        }
    };
}
