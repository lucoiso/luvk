// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Core/Diagnostics.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <span>
#include <string>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/Error.hpp"
#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace
{
    constexpr std::size_t InvalidRecord = std::numeric_limits<std::size_t>::max();

    /** Bit layout of an NV checkpoint marker (a pointer-sized value). */
    constexpr std::uint64_t CheckpointPassBits  = 20U;
    constexpr std::uint64_t CheckpointEndBit    = 20U;
    constexpr std::uint64_t CheckpointStreamBit = 21U;
    constexpr std::uint64_t CheckpointSlotShift = 22U;
    constexpr std::uint64_t CheckpointSlotBits  = 6U;
    constexpr std::uint64_t CheckpointFrameShift = 28U;

    const VkPhysicalDeviceRobustness2FeaturesEXT* FindRobustness2(const luvk::Context& Ctx)
    {
        if (!Ctx.IsDeviceExtensionEnabled(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME))
        {
            return nullptr;
        }
        return Ctx.EnabledFeatures().Find<VkPhysicalDeviceRobustness2FeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT);
    }

    const char* StreamName(const luvk::BreadcrumbStream Stream)
    {
        return Stream == luvk::BreadcrumbStream::Graphics ? "graphics" : "async compute";
    }

    const char* AddressTypeName(const VkDeviceFaultAddressTypeEXT Type)
    {
        switch (Type)
        {
        case VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT: return "none";
        case VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT: return "invalid read";
        case VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT: return "invalid write";
        case VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT: return "invalid execute";
        case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT: return "instruction pointer (unknown)";
        case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT: return "instruction pointer (invalid)";
        case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT: return "instruction pointer (fault)";
        default: return "unknown";
        }
    }

    std::string Hex(const std::uint64_t Value)
    {
        char Buffer[32] = {};
        std::snprintf(Buffer, sizeof(Buffer), "0x%016llx", static_cast<unsigned long long>(Value));
        return Buffer;
    }

    /** Copies a fixed-size, possibly unterminated C string field of a Vulkan structure. */
    template <std::size_t Size>
    std::string FixedString(const char (&Field)[Size])
    {
        return std::string(Field, strnlen(Field, Size));
    }
}

luvk::DiagnosticsModule::DiagnosticsModule(DiagnosticsDesc Desc)
    : m_Desc(Desc)
{
    m_Desc.MaxFramesInFlight = std::clamp(m_Desc.MaxFramesInFlight, 1U, 1U << CheckpointSlotBits);
    m_Desc.MaxPassesPerFrame = std::clamp(m_Desc.MaxPassesPerFrame, 1U, 0xFFFFU);
}

luvk::DiagnosticsModule::~DiagnosticsModule()
{
    DestroyStorage();
}

void luvk::DiagnosticsModule::ConfigureDevice(DeviceRequirements& Requirements)
{
    const auto AddOptional = [&](const std::string_view Extension)
    {
        if (std::ranges::find(Requirements.OptionalExtensions, Extension) == std::end(Requirements.OptionalExtensions))
        {
            Requirements.OptionalExtensions.emplace_back(Extension);
        }
    };

    if (m_Desc.DeviceFault)
    {
        AddOptional(VK_EXT_DEVICE_FAULT_EXTENSION_NAME);
        Requirements.OptionalFeatures
                    .Get<VkPhysicalDeviceFaultFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT, VK_EXT_DEVICE_FAULT_EXTENSION_NAME)
                    .deviceFault = VK_TRUE;
    }

    if (m_Desc.RobustBufferAccess2 || m_Desc.RobustImageAccess2 || m_Desc.NullDescriptor)
    {
        AddOptional(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
        auto& Robustness = Requirements.OptionalFeatures.Get<VkPhysicalDeviceRobustness2FeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT,
                                                                                                      VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
        if (m_Desc.RobustBufferAccess2)
        {
            // robustBufferAccess2 requires robustBufferAccess.
            Requirements.OptionalFeatures.Core().robustBufferAccess = VK_TRUE;
            Robustness.robustBufferAccess2                          = VK_TRUE;
        }
        if (m_Desc.RobustImageAccess2)
        {
            Robustness.robustImageAccess2 = VK_TRUE;
        }
        if (m_Desc.NullDescriptor)
        {
            Robustness.nullDescriptor = VK_TRUE;
        }
    }

    if (m_Desc.Breadcrumbs)
    {
        AddOptional(VK_AMD_BUFFER_MARKER_EXTENSION_NAME);
        AddOptional(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);

        // Device-coherent, uncached memory keeps the markers readable after a hang (the GPU caches are not flushed then).
        // Only on request: the feature changes the memory types every other allocator of the device sees.
        if (m_Desc.DeviceCoherentMemory)
        {
            AddOptional(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME);
            Requirements.OptionalFeatures
                        .Get<VkPhysicalDeviceCoherentMemoryFeaturesAMD>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD,
                                                                        VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME)
                        .deviceCoherentMemory = VK_TRUE;
        }
    }
}

void luvk::DiagnosticsModule::OnContextCreated(Context& Ctx)
{
    if (Ctx.m_Diagnostics != nullptr && Ctx.m_Diagnostics != this)
    {
        throw std::logic_error("[luvk] Only one DiagnosticsModule may be attached to a Context.");
    }

    m_Context         = &Ctx;
    Ctx.m_Diagnostics = this;

    {
        std::scoped_lock Lock(m_Mutex);
        m_Records.assign(static_cast<std::size_t>(m_Desc.MaxFramesInFlight) * 2U, StreamRecord{});
    }

    if (!m_Desc.Breadcrumbs)
    {
        return;
    }

    if (Ctx.IsDeviceExtensionEnabled(VK_AMD_BUFFER_MARKER_EXTENSION_NAME) &&
        (vkCmdWriteBufferMarker2AMD != nullptr || vkCmdWriteBufferMarkerAMD != nullptr))
    {
        const auto* const Coherent = Ctx.EnabledFeatures().Find<VkPhysicalDeviceCoherentMemoryFeaturesAMD>(
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_COHERENT_MEMORY_FEATURES_AMD);
        m_DeviceCoherent = Coherent != nullptr && Coherent->deviceCoherentMemory != VK_FALSE &&
                           Ctx.IsDeviceExtensionEnabled(VK_AMD_DEVICE_COHERENT_MEMORY_EXTENSION_NAME);

        m_MarkerCount = static_cast<std::size_t>(m_Desc.MaxFramesInFlight) * 2U * m_Desc.MaxPassesPerFrame * 2U;

        // The synchronization2 variant is preferred, but VK_LAYER_KHRONOS_validation (1.4.363) silently drops it when
        // VK_KHR_synchronization2 is only core (no marker reaches memory): use the original entry point under validation.
        m_UseMarker2 = vkCmdWriteBufferMarker2AMD != nullptr && (vkCmdWriteBufferMarkerAMD == nullptr || !Ctx.IsValidationEnabled());

        // Written by the graphics and the compute queues: concurrent sharing across every family luvk uses.
        const std::span<const std::uint32_t> Families = Ctx.SharedQueueFamilies();
        const VkBufferCreateInfo             BufferInfo{.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                                        .size                  = m_MarkerCount * sizeof(std::uint32_t),
                                                        .usage                 = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                        .sharingMode           = std::size(Families) > 1U ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
                                                        .queueFamilyIndexCount = std::size(Families) > 1U ? static_cast<std::uint32_t>(std::size(Families)) : 0U,
                                                        .pQueueFamilyIndices   = std::size(Families) > 1U ? std::data(Families) : nullptr};

        VmaAllocationCreateInfo AllocationInfo{.flags         = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                                               .usage         = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
                                               .requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT};
        if (m_DeviceCoherent)
        {
            AllocationInfo.preferredFlags = VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD | VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;
        }

        VmaAllocationInfo Result = {};
        if (vmaCreateBuffer(Ctx.Allocator(), &BufferInfo, &AllocationInfo, &m_Buffer, &m_Allocation, &Result) == VK_SUCCESS &&
            Result.pMappedData != nullptr)
        {
            m_Markers = static_cast<std::uint32_t*>(Result.pMappedData);
            std::memset(m_Markers, 0, m_MarkerCount * sizeof(std::uint32_t));
            Ctx.SetDebugName(m_Buffer, VK_OBJECT_TYPE_BUFFER, "luvk breadcrumbs");
            m_Backend = BreadcrumbBackend::BufferMarkerAMD;
            return;
        }

        DestroyStorage();
        Ctx.EmitMessage(DebugSeverity::Warning, "[luvk] Could not allocate host-visible memory for GPU breadcrumbs.");
    }

    if (Ctx.IsDeviceExtensionEnabled(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME) && vkCmdSetCheckpointNV != nullptr)
    {
        m_Backend = BreadcrumbBackend::CheckpointsNV;
    }
}

void luvk::DiagnosticsModule::OnContextDestroying(Context& Ctx)
{
    DestroyStorage();
    m_Backend = BreadcrumbBackend::None;
    if (Ctx.m_Diagnostics == this)
    {
        Ctx.m_Diagnostics = nullptr;
    }
    m_Context = nullptr;
}

void luvk::DiagnosticsModule::DestroyStorage() noexcept
{
    if (m_Buffer != VK_NULL_HANDLE && m_Context != nullptr)
    {
        vmaDestroyBuffer(m_Context->Allocator(), m_Buffer, m_Allocation);
    }
    m_Buffer      = VK_NULL_HANDLE;
    m_Allocation  = nullptr;
    m_Markers     = nullptr;
    m_MarkerCount = 0U;
}

bool luvk::DiagnosticsModule::IsDeviceFaultAvailable(const Context& Ctx)
{
    const auto* const Fault = Ctx.EnabledFeatures().Find<VkPhysicalDeviceFaultFeaturesEXT>(VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FAULT_FEATURES_EXT);
    return Fault != nullptr && Fault->deviceFault != VK_FALSE && Ctx.IsDeviceExtensionEnabled(VK_EXT_DEVICE_FAULT_EXTENSION_NAME) &&
           vkGetDeviceFaultInfoEXT != nullptr;
}

bool luvk::DiagnosticsModule::IsRobustBufferAccess2Enabled(const Context& Ctx)
{
    const auto* const Robustness = FindRobustness2(Ctx);
    return Robustness != nullptr && Robustness->robustBufferAccess2 != VK_FALSE;
}

bool luvk::DiagnosticsModule::IsRobustImageAccess2Enabled(const Context& Ctx)
{
    const auto* const Robustness = FindRobustness2(Ctx);
    return Robustness != nullptr && Robustness->robustImageAccess2 != VK_FALSE;
}

bool luvk::DiagnosticsModule::IsNullDescriptorEnabled(const Context& Ctx)
{
    const auto* const Robustness = FindRobustness2(Ctx);
    return Robustness != nullptr && Robustness->nullDescriptor != VK_FALSE;
}

bool luvk::DiagnosticsModule::AreBreadcrumbsAvailable(const Context& Ctx)
{
    return Ctx.IsDeviceExtensionEnabled(VK_AMD_BUFFER_MARKER_EXTENSION_NAME) ||
           Ctx.IsDeviceExtensionEnabled(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
}

std::size_t luvk::DiagnosticsModule::RecordIndex(const std::uint32_t Slot, const BreadcrumbStream Stream) const noexcept
{
    if (Slot >= m_Desc.MaxFramesInFlight)
    {
        return InvalidRecord;
    }
    return static_cast<std::size_t>(Slot) * 2U + (Stream == BreadcrumbStream::Graphics ? 0U : 1U);
}

std::uint32_t luvk::DiagnosticsModule::MarkerValue(const std::uint64_t Frame, const std::uint32_t PassIndex) noexcept
{
    return static_cast<std::uint32_t>((Frame & 0xFFFFU) << 16U) | (PassIndex & 0xFFFFU);
}

void luvk::DiagnosticsModule::BeginBreadcrumbs(const std::uint32_t Slot, const BreadcrumbStream Stream, const std::uint64_t Frame)
{
    const std::size_t Index = RecordIndex(Slot, Stream);
    if (Index == InvalidRecord || !HasBreadcrumbs())
    {
        return;
    }

    std::scoped_lock Lock(m_Mutex);
    StreamRecord&    Record = m_Records.at(Index);
    Record.Frame            = Frame;
    Record.Recorded         = true;
    Record.PassCount        = 0U;
}

void luvk::DiagnosticsModule::MarkPass(const VkCommandBuffer  Commands,
                                       const std::uint32_t    Slot,
                                       const BreadcrumbStream Stream,
                                       const std::uint64_t    Frame,
                                       const std::uint32_t    PassIndex,
                                       const std::string_view Name,
                                       const bool             End)
{
    const std::size_t Index = RecordIndex(Slot, Stream);
    if (Index == InvalidRecord || PassIndex >= m_Desc.MaxPassesPerFrame || !HasBreadcrumbs())
    {
        return;
    }

    if (!End)
    {
        std::scoped_lock Lock(m_Mutex);
        StreamRecord&             Record = m_Records.at(Index);
        std::vector<std::string>& Passes = Record.Passes;
        if (std::size(Passes) <= PassIndex)
        {
            Passes.resize(static_cast<std::size_t>(PassIndex) + 1U);
        }
        Passes[PassIndex].assign(Name);
        Record.PassCount = std::max(Record.PassCount, static_cast<std::size_t>(PassIndex) + 1U);
    }

    if (m_Backend == BreadcrumbBackend::BufferMarkerAMD)
    {
        const std::size_t  Marker = (Index * m_Desc.MaxPassesPerFrame + PassIndex) * 2U + (End ? 1U : 0U);
        const VkDeviceSize Offset = Marker * sizeof(std::uint32_t);
        const std::uint32_t Value = MarkerValue(Frame, PassIndex);

        // Begin: written once every previous command started; end: once every previous command completed.
        if (m_UseMarker2)
        {
            vkCmdWriteBufferMarker2AMD(Commands, End ? VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, m_Buffer, Offset, Value);
        }
        else
        {
            vkCmdWriteBufferMarkerAMD(Commands, End ? VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_Buffer, Offset, Value);
        }
    }
    else if (m_Backend == BreadcrumbBackend::CheckpointsNV)
    {
        const std::uint64_t Encoded = (Frame << CheckpointFrameShift) | (static_cast<std::uint64_t>(Slot) << CheckpointSlotShift) |
                                      (static_cast<std::uint64_t>(Stream == BreadcrumbStream::Graphics ? 0U : 1U) << CheckpointStreamBit) |
                                      (static_cast<std::uint64_t>(End ? 1U : 0U) << CheckpointEndBit) | PassIndex;
        vkCmdSetCheckpointNV(Commands, reinterpret_cast<const void*>(static_cast<std::uintptr_t>(Encoded)));
    }
}

luvk::DeviceLostReport luvk::DiagnosticsModule::DescribeBreadcrumbs() const
{
    DeviceLostReport Report;
    if (!HasBreadcrumbs())
    {
        Report.Details = "GPU breadcrumbs: not available (enable DiagnosticsDesc::Breadcrumbs on a device with VK_AMD_buffer_marker or "
                         "VK_NV_device_diagnostic_checkpoints).\n";
        return Report;
    }

    std::scoped_lock Lock(m_Mutex);

    struct Entry
    {
        std::uint64_t    Frame  = 0U;
        std::uint32_t    Slot   = 0U;
        BreadcrumbStream Stream = {BreadcrumbStream::Graphics};
        std::size_t      Record = 0U;
    };

    std::vector<Entry> Entries;
    for (std::size_t Index = 0U; Index < std::size(m_Records); ++Index)
    {
        if (m_Records[Index].Recorded)
        {
            Entries.push_back(Entry{.Frame  = m_Records[Index].Frame,
                                    .Slot   = static_cast<std::uint32_t>(Index / 2U),
                                    .Stream = Index % 2U == 0U ? BreadcrumbStream::Graphics : BreadcrumbStream::AsyncCompute,
                                    .Record = Index});
        }
    }

    // The async compute graph of a frame is submitted before its graphics graph.
    std::ranges::sort(Entries,
                      [](const Entry& A, const Entry& B)
                      {
                          return A.Frame != B.Frame ? A.Frame < B.Frame : A.Stream == BreadcrumbStream::AsyncCompute && B.Stream == BreadcrumbStream::Graphics;
                      });

    // NV checkpoints: the last markers each queue executed.
    std::vector<std::uint64_t> Checkpoints;
    if (m_Backend == BreadcrumbBackend::CheckpointsNV && m_Context != nullptr)
    {
        for (const QueueRole Role : {QueueRole::Graphics, QueueRole::Compute})
        {
            const VkQueue Queue = m_Context->GetQueueInfo(Role).Queue;
            if (Queue == VK_NULL_HANDLE || (Role == QueueRole::Compute && !m_Context->HasAsyncCompute()))
            {
                continue;
            }

            std::uint32_t Count = 0U;
            if (vkGetQueueCheckpointData2NV != nullptr)
            {
                vkGetQueueCheckpointData2NV(Queue, &Count, nullptr);
                std::vector<VkCheckpointData2NV> Data(Count, VkCheckpointData2NV{.sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_2_NV});
                vkGetQueueCheckpointData2NV(Queue, &Count, std::data(Data));
                for (std::uint32_t It = 0U; It < Count; ++It)
                {
                    Checkpoints.push_back(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Data[It].pCheckpointMarker)));
                }
            }
            else if (vkGetQueueCheckpointDataNV != nullptr)
            {
                vkGetQueueCheckpointDataNV(Queue, &Count, nullptr);
                std::vector<VkCheckpointDataNV> Data(Count, VkCheckpointDataNV{.sType = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV});
                vkGetQueueCheckpointDataNV(Queue, &Count, std::data(Data));
                for (std::uint32_t It = 0U; It < Count; ++It)
                {
                    Checkpoints.push_back(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Data[It].pCheckpointMarker)));
                }
            }
        }
    }

    const auto CheckpointReached = [&](const Entry& Current, const std::uint32_t Pass, const bool End)
    {
        // A checkpoint reports the last marker a queue executed; earlier passes of the same frame and stream are past.
        for (const std::uint64_t Value : Checkpoints)
        {
            const std::uint64_t Frame  = Value >> CheckpointFrameShift;
            const std::uint32_t Slot   = static_cast<std::uint32_t>((Value >> CheckpointSlotShift) & ((1U << CheckpointSlotBits) - 1U));
            const bool          Async  = ((Value >> CheckpointStreamBit) & 1U) != 0U;
            const bool          IsEnd  = ((Value >> CheckpointEndBit) & 1U) != 0U;
            const std::uint32_t Marked = static_cast<std::uint32_t>(Value & ((1ULL << CheckpointPassBits) - 1U));
            if (Frame != Current.Frame || Slot != Current.Slot || Async != (Current.Stream == BreadcrumbStream::AsyncCompute))
            {
                continue;
            }
            if (Marked > Pass || (Marked == Pass && (!End || IsEnd)))
            {
                return true;
            }
        }
        return false;
    };

    std::string Details = std::string("GPU breadcrumbs (") +
                          (m_Backend == BreadcrumbBackend::BufferMarkerAMD ? m_DeviceCoherent ? "VK_AMD_buffer_marker, device-coherent memory" : "VK_AMD_buffer_marker"
                                                                           : "VK_NV_device_diagnostic_checkpoints") +
                          "), oldest frame first:\n";

    std::string LastStarted;
    std::string Unfinished;
    std::size_t UnfinishedCount = 0U;

    for (const Entry& Current : Entries)
    {
        const std::vector<std::string>& AllPasses = m_Records[Current.Record].Passes;
        const std::span<const std::string> Passes(AllPasses.data(), std::min(m_Records[Current.Record].PassCount, std::size(AllPasses)));

        std::size_t Begun = 0U;
        std::size_t Ended = 0U;
        std::string Lines;
        for (std::uint32_t Pass = 0U; Pass < std::size(Passes); ++Pass)
        {
            bool Started  = false;
            bool Finished = false;
            if (m_Backend == BreadcrumbBackend::BufferMarkerAMD && m_Markers != nullptr)
            {
                const std::size_t   Marker   = (Current.Record * m_Desc.MaxPassesPerFrame + Pass) * 2U;
                const std::uint32_t Expected = MarkerValue(Current.Frame, Pass);
                Started                      = Marker + 1U < m_MarkerCount && m_Markers[Marker] == Expected;
                Finished                     = Marker + 1U < m_MarkerCount && m_Markers[Marker + 1U] == Expected;
            }
            else
            {
                Started  = CheckpointReached(Current, Pass, false);
                Finished = CheckpointReached(Current, Pass, true);
            }

            Begun += Started ? 1U : 0U;
            Ended += Finished ? 1U : 0U;

            const char* const State = Finished ? "finished" : Started ? "STARTED, NOT FINISHED" : "not started";
            char              Line[64] = {};
            std::snprintf(Line, sizeof(Line), "    [%4u] %-22s ", Pass, State);
            Lines += Line;
            Lines += Passes[Pass];
            Lines += '\n';

            const std::string Label = "'" + Passes[Pass] + "' (frame " + std::to_string(Current.Frame) + ", " + StreamName(Current.Stream) + ")";
            if (Started)
            {
                LastStarted = Label;
            }
            if (Started && !Finished)
            {
                if (UnfinishedCount < 8U)
                {
                    Unfinished += std::empty(Unfinished) ? Label : ", " + Label;
                }
                ++UnfinishedCount;
            }
        }

        Details += "  Frame " + std::to_string(Current.Frame) + " (slot " + std::to_string(Current.Slot) + ", " + StreamName(Current.Stream) + " graph): " +
                   std::to_string(std::size(Passes)) + " passes, " + std::to_string(Begun) + " started, " + std::to_string(Ended) + " finished\n";
        Details += Lines;
    }

    if (std::empty(Entries))
    {
        Details += "  No frame was recorded with breadcrumbs.\n";
    }

    if (!std::empty(LastStarted))
    {
        Report.Summary = "Last GPU pass started: " + LastStarted + ".";
        if (UnfinishedCount > 0U)
        {
            Report.Summary += "\nStarted but not finished: " + Unfinished + (UnfinishedCount > 8U ? ", ..." : "") + ".";
        }
        else
        {
            Report.Summary += "\nEvery started pass finished: the fault happened outside the render graph passes (uploads, presentation, "
                              "third-party submissions) or the markers did not reach memory.";
        }
    }
    else if (!std::empty(Entries))
    {
        Report.Summary = "No breadcrumb of the frames in flight reached memory (the GPU hung before the first pass, or the markers were lost).";
    }

    Report.Details = std::move(Details);
    return Report;
}

luvk::DeviceLostReport luvk::DiagnosticsModule::DescribeDeviceFault(const Context& Ctx)
{
    DeviceLostReport Report;
    if (!IsDeviceFaultAvailable(Ctx))
    {
        Report.Details = "Device fault information: not available (VK_EXT_device_fault is not enabled on this device).\n";
        return Report;
    }

    VkDeviceFaultCountsEXT Counts{.sType = VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT};
    VkResult               Result = vkGetDeviceFaultInfoEXT(Ctx.Device(), &Counts, nullptr);
    if (Result != VK_SUCCESS && Result != VK_INCOMPLETE)
    {
        Report.Details = std::string("Device fault information: vkGetDeviceFaultInfoEXT failed with ") + ResultName(Result) + ".\n";
        return Report;
    }

    std::vector<VkDeviceFaultAddressInfoEXT> Addresses(Counts.addressInfoCount);
    std::vector<VkDeviceFaultVendorInfoEXT>  Vendors(Counts.vendorInfoCount);
    const VkDeviceSize                       BinarySize = Counts.vendorBinarySize;
    Counts.vendorBinarySize                             = 0U;

    VkDeviceFaultInfoEXT Info{.sType         = VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT,
                              .pAddressInfos = std::empty(Addresses) ? nullptr : std::data(Addresses),
                              .pVendorInfos  = std::empty(Vendors) ? nullptr : std::data(Vendors)};
    Result = vkGetDeviceFaultInfoEXT(Ctx.Device(), &Counts, &Info);
    if (Result != VK_SUCCESS && Result != VK_INCOMPLETE)
    {
        Report.Details = std::string("Device fault information: vkGetDeviceFaultInfoEXT failed with ") + ResultName(Result) + ".\n";
        return Report;
    }

    const std::string Description = FixedString(Info.description);
    Report.Summary                = std::empty(Description) ? std::string{} : "Driver fault description: " + Description + ".";

    std::string Details = "Device fault information (VK_EXT_device_fault):\n";
    Details += "  Description: " + (std::empty(Description) ? std::string("(none)") : Description) + '\n';

    for (std::uint32_t Index = 0U; Index < std::min<std::uint32_t>(Counts.addressInfoCount, static_cast<std::uint32_t>(std::size(Addresses))); ++Index)
    {
        const VkDeviceFaultAddressInfoEXT& Address   = Addresses[Index];
        const VkDeviceSize                 Precision = std::max<VkDeviceSize>(Address.addressPrecision, 1U);
        const VkDeviceAddress              Low       = Address.reportedAddress & ~(Precision - 1U);
        const VkDeviceAddress              High      = Address.reportedAddress | (Precision - 1U);
        Details += "  Address " + std::to_string(Index) + ": " + AddressTypeName(Address.addressType) + " at " + Hex(Address.reportedAddress) +
                   " (precision " + std::to_string(Precision) + " bytes, range " + Hex(Low) + " - " + Hex(High) + ")\n";

        if (Index == 0U && Address.addressType != VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT)
        {
            Report.Summary += (std::empty(Report.Summary) ? "" : "\n") + std::string("Faulting address: ") + AddressTypeName(Address.addressType) + " at " +
                              Hex(Address.reportedAddress) + ".";
        }
    }

    for (std::uint32_t Index = 0U; Index < std::min<std::uint32_t>(Counts.vendorInfoCount, static_cast<std::uint32_t>(std::size(Vendors))); ++Index)
    {
        const VkDeviceFaultVendorInfoEXT& Vendor = Vendors[Index];
        Details += "  Vendor info " + std::to_string(Index) + ": code " + Hex(Vendor.vendorFaultCode) + ", data " + Hex(Vendor.vendorFaultData) + ", " +
                   FixedString(Vendor.description) + '\n';
    }

    if (BinarySize > 0U)
    {
        Details += "  Vendor binary crash dump: " + std::to_string(BinarySize) + " bytes available (not captured).\n";
    }

    Report.Details = std::move(Details);
    return Report;
}
