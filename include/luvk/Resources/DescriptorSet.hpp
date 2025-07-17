// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Module.hpp"

#include <volk/volk.h>
#include <span>
#include <memory>

namespace luvk
{
    /** Render module responsible for descriptor set management */
    class Device;
    class DescriptorPool;
    class Memory;

    class LUVKMODULE_API DescriptorSet
    {
        /** Layout used by this set */
        VkDescriptorSetLayout m_Layout{VK_NULL_HANDLE};

        /** Descriptor set handle */
        VkDescriptorSet m_Set{VK_NULL_HANDLE};

        /** Device module used for allocation */
        std::shared_ptr<Device> m_DeviceModule{};

        /** Descriptor pool used to allocate the set */
        std::shared_ptr<DescriptorPool> m_PoolModule{};

        /** Memory module used for descriptor allocation */
        std::shared_ptr<Memory> m_MemoryModule{};

        /** True when this object created the layout */
        bool m_OwnsLayout{false};

    public: /** Default constructor */
        constexpr DescriptorSet() = default;

        /** Destructor cleans up resources */
        ~DescriptorSet();

        /** Information to create a layout */
        struct LayoutInfo
        {
            /** Descriptor bindings used by the layout */
            std::span<VkDescriptorSetLayoutBinding const> Bindings{};
        };

        /** Create descriptor set layout */
        void CreateLayout(std::shared_ptr<Device> const& DeviceModule, LayoutInfo const& Info);

        /** Use existing descriptor set layout */
        void UseLayout(std::shared_ptr<Device> const& DeviceModule, VkDescriptorSetLayout Layout);

        /** Allocate descriptor set from pool */
        void Allocate(std::shared_ptr<Device> const& DeviceModule,
                      std::shared_ptr<DescriptorPool> const& PoolModule,
                      std::shared_ptr<Memory> const& MemoryModule = nullptr);

        /** Update a buffer binding */
        void UpdateBuffer(std::shared_ptr<Device> const& DeviceModule,
                          VkBuffer Buffer,
                          VkDeviceSize Size,
                          std::uint32_t Binding,
                          VkDescriptorType Type) const;

        /** Update an image binding */
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

    private: /** Release the descriptor set and layout */
        void Destroy();
    };
} // namespace luvk
