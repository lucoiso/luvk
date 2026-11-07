// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstddef>
#include <memory>
#include <new>
#include <span>
#include <string>
#include <type_traits>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    /**
     * Owning, typed pNext chain of Vulkan *feature* structures.
     *
     * Every feature structure is a VkBaseOutStructure header followed only by VkBool32 members, which
     * allows generic set operations (merge, intersect, subset checks) without per-struct code. Each entry
     * can be tied to the device extension that introduces it, so it is only chained when that extension
     * is enabled; Vulkan 1.4 core features are only chained on 1.4 devices.
     */
    class LUVK_API FeatureChain
    {
        /** One owned feature structure in the pNext chain. */
        struct Entry
        {
            /** VkStructureType of the stored structure. */
            VkStructureType SType = {};

            /** sizeof(T) of the stored structure in bytes (allocation size). */
            std::size_t Size = 0U;

            /** Device extension that provides the structure (empty for core structures). */
            std::string Extension;

            /** Byte storage holding the constructed structure. */
            std::unique_ptr<std::byte[]> Data;
        };

        /** Owned entries in chain order (VkPhysicalDeviceFeatures2 always first). */
        std::vector<Entry> m_Entries;

    public:
        /** Constructor: starts the chain with VkPhysicalDeviceFeatures2. */
        FeatureChain();

        /** Destructor. */
        ~FeatureChain() = default;

        /** Copy constructor: deep-copies every entry. */
        FeatureChain(const FeatureChain& Other);

        /** Copy assignment: deep-copies every entry. */
        FeatureChain& operator=(const FeatureChain& Other);

        /** Move constructor: transfers the entries. */
        FeatureChain(FeatureChain&&) noexcept            = default;

        /** Move assignment: transfers the entries. */
        FeatureChain& operator=(FeatureChain&&) noexcept = default;

        /**
         * Returns the structure of type T, creating it zero-initialised on first access.
         * Extension: name of the device extension that provides T (empty for core structures).
         */
        template <typename T>
        T& Get(const VkStructureType SType, const std::string_view Extension = {})
        {
            static_assert(std::is_trivially_copyable_v<T>, "Vulkan feature structures are trivially copyable");
            static_assert(sizeof(T) >= sizeof(VkBaseOutStructure), "Not a Vulkan structure");

            if (Entry* const Existing = FindEntry(SType))
            {
                return *std::launder(reinterpret_cast<T*>(Existing->Data.get()));
            }

            Entry& NewEntry = AddEntry(SType, sizeof(T), Extension);
            T* const Value  = ::new (static_cast<void*>(NewEntry.Data.get())) T{};
            Value->sType    = SType;
            return *Value;
        }

        /** Returns a const pointer to the structure of type T, or nullptr when the chain has no such entry. */
        template <typename T>
        [[nodiscard]] const T* Find(const VkStructureType SType) const noexcept
        {
            if (const Entry* const Existing = FindEntry(SType))
            {
                return std::launder(reinterpret_cast<const T*>(Existing->Data.get()));
            }
            return nullptr;
        }

        /** Vulkan 1.0 core features. */
        [[nodiscard]] VkPhysicalDeviceFeatures&         Core();

        /** Vulkan 1.1 core features. */
        [[nodiscard]] VkPhysicalDeviceVulkan11Features& Vulkan11();

        /** Vulkan 1.2 core features. */
        [[nodiscard]] VkPhysicalDeviceVulkan12Features& Vulkan12();

        /** Vulkan 1.3 core features. */
        [[nodiscard]] VkPhysicalDeviceVulkan13Features& Vulkan13();

        /** Vulkan 1.4 core features (chained only on 1.4 devices). */
        [[nodiscard]] VkPhysicalDeviceVulkan14Features& Vulkan14();

        /** Const access to the Vulkan 1.0 core features (all-false when absent). */
        [[nodiscard]] const VkPhysicalDeviceFeatures&         Core() const;

        /** Const access to the Vulkan 1.1 core features (all-false when absent). */
        [[nodiscard]] const VkPhysicalDeviceVulkan11Features& Vulkan11() const;

        /** Const access to the Vulkan 1.2 core features (all-false when absent). */
        [[nodiscard]] const VkPhysicalDeviceVulkan12Features& Vulkan12() const;

        /** Const access to the Vulkan 1.3 core features (all-false when absent). */
        [[nodiscard]] const VkPhysicalDeviceVulkan13Features& Vulkan13() const;

        /** True when this entry may be used given the device API version and enabled extensions. */
        [[nodiscard]] static bool IsEntryUsable(VkStructureType                  SType,
                                                std::string_view                 Extension,
                                                std::uint32_t                    ApiVersion,
                                                std::span<const std::string>     EnabledExtensions);

        /**
         * Links the usable, non-empty entries (VkPhysicalDeviceFeatures2 always first) and returns the head.
         * The returned pointer is valid until the chain is modified.
         */
        [[nodiscard]] void* Link(std::uint32_t ApiVersion, std::span<const std::string> EnabledExtensions);

        /** First structure (VkPhysicalDeviceFeatures2) without relinking - use with chains from MakeQuery(). */
        [[nodiscard]] void* Head() noexcept
        {
            return m_Entries.front().Data.get();
        }

        /** Zeroed copy with the same usable structure types, ready for vkGetPhysicalDeviceFeatures2. */
        [[nodiscard]] FeatureChain MakeQuery(std::uint32_t ApiVersion, std::span<const std::string> EnabledExtensions) const;

        /** Adds every structure type of Other (zeroed) that is not present yet. */
        void AddTypesFrom(const FeatureChain& Other);

        /** this |= Other */
        void Merge(const FeatureChain& Other);

        /** this |= (Optional & Supported) */
        void MergeSupported(const FeatureChain& Optional, const FeatureChain& Supported);

        /** True when every requested VkBool32 is also set in Supported. Missing items are described in OutMissing. */
        [[nodiscard]] bool IsSupportedBy(const FeatureChain& Supported, std::vector<std::string>* OutMissing = nullptr) const;

        /** True when no VkBool32 of the given entry is set. */
        [[nodiscard]] bool IsEmpty(VkStructureType SType) const noexcept;

    private:
        /** Returns the mutable entry with the given SType, or nullptr. */
        [[nodiscard]] Entry*       FindEntry(VkStructureType SType) noexcept;

        /** Returns the entry with the given SType, or nullptr. */
        [[nodiscard]] const Entry* FindEntry(VkStructureType SType) const noexcept;

        /** Appends a zeroed entry of the given size tied to Extension and returns it. */
        Entry&                     AddEntry(VkStructureType SType, std::size_t Size, std::string_view Extension);

        /** Number of VkBool32 members in the entry's structure. */
        [[nodiscard]] static std::size_t   BoolCount(const Entry& Value) noexcept;

        /** Reads the Index-th VkBool32 of the entry. */
        [[nodiscard]] static VkBool32      ReadBool(const Entry& Value, std::size_t Index) noexcept;

        /** Writes the Index-th VkBool32 of the entry. */
        static void                        WriteBool(Entry& Value, std::size_t Index, VkBool32 State) noexcept;

        /** True when every VkBool32 of the entry is zero. */
        [[nodiscard]] static bool          IsEmpty(const Entry& Value) noexcept;
    };

    /** Instance extensions/layers requested before the instance is created. */
    struct InstanceRequirements
    {
        /** Required extensions; a missing one aborts Context creation. */
        std::vector<std::string> Extensions;

        /** Enabled only when the loader reports them. */
        std::vector<std::string> OptionalExtensions;

        /** Required layers; a missing one aborts Context creation. */
        std::vector<std::string> Layers;
    };

    /** Device extensions/features requested when selecting and creating the device. */
    struct DeviceRequirements
    {
        /** Required extensions; a missing one rejects a physical device. */
        std::vector<std::string> Extensions;

        /** Enabled only when the physical device supports them. */
        std::vector<std::string> OptionalExtensions;

        /** Features that must be supported (hard requirement). */
        FeatureChain Features = {};

        /** Features enabled when supported (soft requirement). */
        FeatureChain OptionalFeatures = {};
    };
}
