/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <span>
#include <volk.h>

namespace luvk
{
    class Device;
    class DescriptorPool;

    /**
     * Wrapper for a Vulkan Descriptor Set and its Layout.
     */
    class LUVK_API DescriptorSet
    {
        /** The Vulkan descriptor set layout handle. */
        VkDescriptorSetLayout m_Layout{VK_NULL_HANDLE};

        /** The allocated Vulkan descriptor set handle. */
        VkDescriptorSet m_Set{VK_NULL_HANDLE};

        /** Pointer to the Device module for getting the logical device. */
        Device* m_DeviceModule{nullptr};

        /** Pointer to the DescriptorPool module for allocation. */
        DescriptorPool* m_PoolModule{nullptr};

        /** Flag indicating if the descriptor set has been allocated from the pool. */
        bool m_IsAllocated{false};

    public:
        /** Descriptor sets cannot be default constructed. */
        DescriptorSet() = delete;

        /**
         * Constructor.
         * @param DeviceModule Pointer to the Device module.
         * @param PoolModule Pointer to the DescriptorPool module.
         */
        explicit DescriptorSet(Device* DeviceModule, DescriptorPool* PoolModule);

        /** Destructor (destroys the layout). The set is freed by the pool reset. */
        ~DescriptorSet();

        /**
         * Creates the VkDescriptorSetLayout from a span of bindings.
         * @param Bindings A span of VkDescriptorSetLayoutBinding structures.
         */
        void CreateLayout(std::span<const VkDescriptorSetLayoutBinding> Bindings);

        /**
         * Allocates the VkDescriptorSet from the DescriptorPool module.
         */
        void Allocate();

        /**
         * Updates a uniform buffer binding in the descriptor set.
         * @param Binding The binding number.
         * @param Buffer The VkBuffer handle.
         * @param Range The range of the buffer accessible in the shader.
         * @param Offset The offset in the buffer.
         */
        void UpdateUniformBuffer(std::uint32_t Binding, VkBuffer Buffer, VkDeviceSize Range, VkDeviceSize Offset = 0U) const;

        /**
         * Updates a storage buffer binding in the descriptor set.
         * @param Binding The binding number.
         * @param Buffer The VkBuffer handle.
         * @param Range The range of the buffer accessible in the shader.
         * @param Offset The offset in the buffer.
         */
        void UpdateStorageBuffer(std::uint32_t Binding, VkBuffer Buffer, VkDeviceSize Range, VkDeviceSize Offset = 0U) const;

        /**
         * Updates a combined image sampler binding in the descriptor set.
         * @param Binding The binding number.
         * @param ImageView The VkImageView handle.
         * @param Sampler The VkSampler handle.
         * @param Layout The image layout the image is currently in.
         */
        void UpdateImage(std::uint32_t Binding,
                         VkImageView   ImageView,
                         VkSampler     Sampler,
                         VkImageLayout Layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;

        /** Get the allocated VkDescriptorSet handle. */
        [[nodiscard]] constexpr VkDescriptorSet GetHandle() const noexcept
        {
            return m_Set;
        }

        /** Get the VkDescriptorSetLayout handle. */
        [[nodiscard]] constexpr VkDescriptorSetLayout GetLayout() const noexcept
        {
            return m_Layout;
        }
    };
}
