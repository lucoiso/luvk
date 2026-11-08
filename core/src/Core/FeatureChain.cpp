// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Vulkan.hpp"
#include <algorithm>
#include <cstring>

namespace
{
    constexpr std::size_t HeaderSize = sizeof(VkBaseOutStructure);

    std::string DescribeSType(const VkStructureType SType)
    {
        switch (SType)
        {
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2: return "VkPhysicalDeviceFeatures";
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES: return "VkPhysicalDeviceVulkan11Features";
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES: return "VkPhysicalDeviceVulkan12Features";
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES: return "VkPhysicalDeviceVulkan13Features";
        case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES: return "VkPhysicalDeviceVulkan14Features";
        default: return "VkStructureType(" + std::to_string(static_cast<int>(SType)) + ")";
        }
    }

    bool Contains(const std::span<const std::string> Values, const std::string_view Value)
    {
        return std::ranges::find(Values, Value) != std::end(Values);
    }
}

luvk::FeatureChain::FeatureChain()
{

    (void)Get<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2);
}

luvk::FeatureChain::FeatureChain(const FeatureChain& Other)
{
    *this = Other;
}

luvk::FeatureChain& luvk::FeatureChain::operator=(const FeatureChain& Other)
{
    if (this == &Other)
    {
        return *this;
    }

    m_Entries.clear();
    m_Entries.reserve(std::size(Other.m_Entries));
    for (const Entry& It : Other.m_Entries)
    {
        Entry& Copy = AddEntry(It.SType, It.Size, It.Extension);
        std::memcpy(Copy.Data.get(), It.Data.get(), It.Size);
        reinterpret_cast<VkBaseOutStructure*>(Copy.Data.get())->pNext = nullptr;
    }
    return *this;
}

VkPhysicalDeviceFeatures& luvk::FeatureChain::Core()
{
    return Get<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2).features;
}

VkPhysicalDeviceVulkan11Features& luvk::FeatureChain::Vulkan11()
{
    return Get<VkPhysicalDeviceVulkan11Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
}

VkPhysicalDeviceVulkan12Features& luvk::FeatureChain::Vulkan12()
{
    return Get<VkPhysicalDeviceVulkan12Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
}

VkPhysicalDeviceVulkan13Features& luvk::FeatureChain::Vulkan13()
{
    return Get<VkPhysicalDeviceVulkan13Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
}

VkPhysicalDeviceVulkan14Features& luvk::FeatureChain::Vulkan14()
{
    return Get<VkPhysicalDeviceVulkan14Features>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES);
}

const VkPhysicalDeviceFeatures& luvk::FeatureChain::Core() const
{
    return Find<VkPhysicalDeviceFeatures2>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2)->features;
}

namespace
{
    template <typename T>
    const T& FindOrEmpty(const luvk::FeatureChain& Chain, const VkStructureType SType)
    {
        static constexpr T Empty = {};
        const T* const     Found = Chain.Find<T>(SType);
        return Found != nullptr ? *Found : Empty;
    }
}

const VkPhysicalDeviceVulkan11Features& luvk::FeatureChain::Vulkan11() const
{
    return FindOrEmpty<VkPhysicalDeviceVulkan11Features>(*this, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES);
}

const VkPhysicalDeviceVulkan12Features& luvk::FeatureChain::Vulkan12() const
{
    return FindOrEmpty<VkPhysicalDeviceVulkan12Features>(*this, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES);
}

const VkPhysicalDeviceVulkan13Features& luvk::FeatureChain::Vulkan13() const
{
    return FindOrEmpty<VkPhysicalDeviceVulkan13Features>(*this, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES);
}

bool luvk::FeatureChain::IsEntryUsable(const VkStructureType              SType,
                                       const std::string_view             Extension,
                                       const std::uint32_t                ApiVersion,
                                       const std::span<const std::string> EnabledExtensions)
{
    if (SType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_4_FEATURES && ApiVersion < VK_API_VERSION_1_4)
    {
        return false;
    }

    return std::empty(Extension) || Contains(EnabledExtensions, Extension);
}

void* luvk::FeatureChain::Link(const std::uint32_t ApiVersion, const std::span<const std::string> EnabledExtensions)
{
    VkBaseOutStructure* Head = nullptr;
    VkBaseOutStructure* Tail = nullptr;

    for (Entry& It : m_Entries)
    {
        auto* const Base = reinterpret_cast<VkBaseOutStructure*>(It.Data.get());
        Base->pNext      = nullptr;

        const bool IsHead = It.SType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
        if (!IsHead && (IsEmpty(It) || !IsEntryUsable(It.SType, It.Extension, ApiVersion, EnabledExtensions)))
        {
            continue;
        }

        if (Head == nullptr)
        {
            Head = Base;
        }
        else
        {
            Tail->pNext = Base;
        }
        Tail = Base;
    }

    return Head;
}

luvk::FeatureChain luvk::FeatureChain::MakeQuery(const std::uint32_t ApiVersion, const std::span<const std::string> EnabledExtensions) const
{
    FeatureChain Query;
    for (const Entry& It : m_Entries)
    {
        if (IsEntryUsable(It.SType, It.Extension, ApiVersion, EnabledExtensions) && Query.FindEntry(It.SType) == nullptr)
        {
            Query.AddEntry(It.SType, It.Size, It.Extension);
        }
    }

    VkBaseOutStructure* Previous = nullptr;
    for (Entry& It : Query.m_Entries)
    {
        auto* const Base = reinterpret_cast<VkBaseOutStructure*>(It.Data.get());
        if (Previous != nullptr)
        {
            Previous->pNext = Base;
        }
        Previous = Base;
    }

    return Query;
}

void luvk::FeatureChain::AddTypesFrom(const FeatureChain& Other)
{
    for (const Entry& It : Other.m_Entries)
    {
        if (FindEntry(It.SType) == nullptr)
        {
            AddEntry(It.SType, It.Size, It.Extension);
        }
    }
}

void luvk::FeatureChain::Merge(const FeatureChain& Other)
{
    for (const Entry& Source : Other.m_Entries)
    {
        Entry* Target = FindEntry(Source.SType);
        if (Target == nullptr)
        {
            Target = &AddEntry(Source.SType, Source.Size, Source.Extension);
        }

        for (std::size_t Index = 0U; Index < BoolCount(Source); ++Index)
        {
            if (ReadBool(Source, Index) != VK_FALSE)
            {
                WriteBool(*Target, Index, VK_TRUE);
            }
        }
    }
}

void luvk::FeatureChain::MergeSupported(const FeatureChain& Optional, const FeatureChain& Supported)
{
    for (const Entry& Source : Optional.m_Entries)
    {
        const Entry* const Available = Supported.FindEntry(Source.SType);
        if (Available == nullptr)
        {
            continue;
        }

        Entry* Target = FindEntry(Source.SType);
        if (Target == nullptr)
        {
            Target = &AddEntry(Source.SType, Source.Size, Source.Extension);
        }

        for (std::size_t Index = 0U; Index < BoolCount(Source); ++Index)
        {
            if (ReadBool(Source, Index) != VK_FALSE && ReadBool(*Available, Index) != VK_FALSE)
            {
                WriteBool(*Target, Index, VK_TRUE);
            }
        }
    }
}

bool luvk::FeatureChain::IsSupportedBy(const FeatureChain& Supported, std::vector<std::string>* const OutMissing) const
{
    bool Result = true;

    for (const Entry& Requested : m_Entries)
    {
        if (IsEmpty(Requested))
        {
            continue;
        }

        const Entry* const Available = Supported.FindEntry(Requested.SType);
        for (std::size_t Index = 0U; Index < BoolCount(Requested); ++Index)
        {
            if (ReadBool(Requested, Index) == VK_FALSE)
            {
                continue;
            }

            if (Available == nullptr || ReadBool(*Available, Index) == VK_FALSE)
            {
                Result = false;
                if (OutMissing != nullptr)
                {
                    OutMissing->push_back(DescribeSType(Requested.SType) + " member #" + std::to_string(Index));
                }
            }
        }
    }

    return Result;
}

bool luvk::FeatureChain::IsEmpty(const VkStructureType SType) const noexcept
{
    const Entry* const Found = FindEntry(SType);
    return Found == nullptr || IsEmpty(*Found);
}

luvk::FeatureChain::Entry* luvk::FeatureChain::FindEntry(const VkStructureType SType) noexcept
{
    const auto It = std::ranges::find(m_Entries, SType, &Entry::SType);
    return It != std::end(m_Entries) ? &*It : nullptr;
}

const luvk::FeatureChain::Entry* luvk::FeatureChain::FindEntry(const VkStructureType SType) const noexcept
{
    const auto It = std::ranges::find(m_Entries, SType, &Entry::SType);
    return It != std::end(m_Entries) ? &*It : nullptr;
}

luvk::FeatureChain::Entry& luvk::FeatureChain::AddEntry(const VkStructureType SType, const std::size_t Size, const std::string_view Extension)
{
    Entry NewEntry{.SType = SType, .Size = Size, .Extension = std::string(Extension), .Data = std::make_unique<std::byte[]>(Size)};
    std::memset(NewEntry.Data.get(), 0, Size);
    std::memcpy(NewEntry.Data.get(), &SType, sizeof(SType));
    return m_Entries.emplace_back(std::move(NewEntry));
}

std::size_t luvk::FeatureChain::BoolCount(const Entry& Value) noexcept
{
    return (Value.Size - HeaderSize) / sizeof(VkBool32);
}

VkBool32 luvk::FeatureChain::ReadBool(const Entry& Value, const std::size_t Index) noexcept
{
    VkBool32 Result = VK_FALSE;
    std::memcpy(&Result, Value.Data.get() + HeaderSize + Index * sizeof(VkBool32), sizeof(VkBool32));
    return Result;
}

void luvk::FeatureChain::WriteBool(Entry& Value, const std::size_t Index, const VkBool32 State) noexcept
{
    std::memcpy(Value.Data.get() + HeaderSize + Index * sizeof(VkBool32), &State, sizeof(VkBool32));
}

bool luvk::FeatureChain::IsEmpty(const Entry& Value) noexcept
{
    for (std::size_t Index = 0U; Index < BoolCount(Value); ++Index)
    {
        if (ReadBool(Value, Index) != VK_FALSE)
        {
            return false;
        }
    }
    return true;
}
