// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    class Context;
    struct BindlessLimits;

    /** Binding numbers of the global bindless descriptor set (set = 0). */
    enum class BindlessSlot : std::uint8_t
    {
        SampledImage = 0,
        Sampler      = 1,
        StorageImage = 2,
        AccelerationStructure = 3,
    };

    /**
     * One global descriptor set with update-after-bind, partially bound descriptor arrays, plus the single
     * pipeline layout shared by every luvk pipeline (that set + 128 bytes of push constants for all stages).
     *
     * Buffers are not in the heap: shaders access them through buffer device addresses passed in push
     * constants. This removes per-material descriptor sets/layouts entirely.
     */
    class LUVK_API BindlessHeap
    {
        /** Free-list plus bump pointer backing one bindless binding array. */
        struct IndexAllocator
        {
            /** Indices returned by Free() and handed out again by the next Allocate(). */
            std::vector<std::uint32_t> FreeList;

            /** Next never-allocated index (bump pointer). */
            std::uint32_t Next = 0U;

            /** Maximum number of indices for this slot type (clamped to the device's update-after-bind limits). */
            std::uint32_t Capacity = 0U;
        };

        /** Context providing the device used to create/destroy the Vulkan objects. */
        Context&                      m_Context;

        /** One allocator per BindlessSlot, indexed by the enumerator value. */
        std::array<IndexAllocator, 4> m_Allocators;

        /** Serialises descriptor writes and index allocation (Allocate/Free/Write are thread-safe). */
        mutable std::mutex m_Mutex;

        /** Bindless set layout (update-after-bind and partially bound flags on every binding). */
        VkDescriptorSetLayout m_SetLayout = VK_NULL_HANDLE;

        /** Descriptor pool backing the single bindless set. */
        VkDescriptorPool m_Pool = VK_NULL_HANDLE;

        /** The global descriptor set bound at set 0. */
        VkDescriptorSet m_Set = VK_NULL_HANDLE;

        /** Pipeline layout shared by every luvk pipeline (bindless set + push constants). */
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;

    public:
        /** Constructor: creates the layout, pool, descriptor set and shared pipeline layout from Limits. */
        BindlessHeap(Context& Ctx, const BindlessLimits& Limits);

        /** Destructor. */
        ~BindlessHeap();

        /** Deleted copy constructor. */
        BindlessHeap(const BindlessHeap&)            = delete;

        /** Deleted copy assignment. */
        BindlessHeap& operator=(const BindlessHeap&) = delete;

        /** Reserves a slot. Thread-safe. Throws when the heap is full. */
        [[nodiscard]] std::uint32_t Allocate(BindlessSlot Slot);

        /** Frees a slot immediately. Slots the GPU may still read must go through Context::Retire (RetiredKind::BindlessSlot). */
        void Free(BindlessSlot Slot, std::uint32_t Index) noexcept;

        /** Writes an image view (with its layout, default VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) into the sampled image array. */
        void WriteSampledImage(std::uint32_t Index, VkImageView View, VkImageLayout Layout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL) const;

        /** Writes a sampler into the sampler array. */
        void WriteSampler(std::uint32_t Index, VkSampler Sampler) const;

        /** Writes a storage image view (VK_IMAGE_LAYOUT_GENERAL) into the storage image array. */
        void WriteStorageImage(std::uint32_t Index, VkImageView View) const;

        /** Writes an acceleration structure into the AS array (throws when ray tracing is not enabled). */
        void WriteAccelerationStructure(std::uint32_t Index, VkAccelerationStructureKHR Structure) const;

        /** Maximum number of indices for Slot (clamped to the device's update-after-bind limits). */
        [[nodiscard]] std::uint32_t Capacity(BindlessSlot Slot) const noexcept;

        /** Number of currently allocated (not yet freed) indices for Slot. */
        [[nodiscard]] std::uint32_t InUse(BindlessSlot Slot) const noexcept;

        /** Bindless descriptor set layout. */
        [[nodiscard]] VkDescriptorSetLayout SetLayout() const noexcept { return m_SetLayout; }

        /** The global descriptor set (bound at set 0). */
        [[nodiscard]] VkDescriptorSet       Set() const noexcept { return m_Set; }

        /** Shared pipeline layout (bindless set + push constants). */
        [[nodiscard]] VkPipelineLayout      PipelineLayout() const noexcept { return m_PipelineLayout; }

    private:
        /** Destroys the pipeline layout, descriptor pool and set layout in reverse creation order. */
        void Release() noexcept;

        /** Writes an image descriptor (sampled image or sampler) into Slot's array; throws when Index is out of range. */
        void Write(BindlessSlot Slot, std::uint32_t Index, const VkDescriptorImageInfo& Info) const;
    };
}
