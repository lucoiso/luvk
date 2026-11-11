// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include "luvk/Core/DiagnosticsTypes.hpp"
#include "luvk/Core/Module.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace luvk
{
    class Context;

    /**
     * Crash diagnostics: optional VK_EXT_device_fault, VK_EXT_robustness2 and GPU breadcrumbs (VK_AMD_buffer_marker or
     * VK_NV_device_diagnostic_checkpoints, plus VK_AMD_device_coherent_memory so markers survive a hang).
     *
     * Add it to ContextDesc::Modules (the app layer does it when ApplicationDesc::EnableDiagnostics is set). The Renderer
     * then has the render graphs write a marker before and after every executed pass, value
     * (frame number << 16 | pass index), and keeps the pass names of the frames in flight; when luvk observes
     * VK_ERROR_DEVICE_LOST, Context::ReportDeviceLost combines the fault information and the breadcrumbs into a
     * DeviceLostReport. At most one DiagnosticsModule may be attached to a Context.
     */
    class LUVK_API DiagnosticsModule final : public Module
    {
        /** Pass names recorded for one frame slot and one graph. */
        struct StreamRecord
        {
            /** Frame number the record belongs to. */
            std::uint64_t Frame = 0U;

            /** True once a frame was recorded in this slot. */
            bool Recorded = false;

            /** Names of the executed passes, by breadcrumb pass index (the first PassCount are this record's; the strings
             *  keep their storage from frame to frame). */
            std::vector<std::string> Passes;

            /** Passes recorded for this frame. */
            std::size_t PassCount = 0U;
        };

        /** Options requested by the application. */
        DiagnosticsDesc m_Desc;

        /** Context the module is attached to (between OnContextCreated and OnContextDestroying). */
        Context* m_Context = nullptr;

        /** Breadcrumb mechanism selected at OnContextCreated. */
        BreadcrumbBackend m_Backend = {BreadcrumbBackend::None};

        /** Marker buffer (BufferMarkerAMD only). */
        VkBuffer m_Buffer = nullptr;

        /** Allocation of the marker buffer. */
        VmaAllocation m_Allocation = nullptr;

        /** Persistently mapped markers (two per pass: begin and end). */
        std::uint32_t* m_Markers = nullptr;

        /** Number of markers in m_Markers. */
        std::size_t m_MarkerCount = 0U;

        /** True when the marker memory is device coherent (VK_AMD_device_coherent_memory). */
        bool m_DeviceCoherent = false;

        /** Uses vkCmdWriteBufferMarker2AMD (synchronization2) instead of vkCmdWriteBufferMarkerAMD. */
        bool m_UseMarker2 = false;

        /** One record per frame slot and stream (slot-major). */
        std::vector<StreamRecord> m_Records;

        /** Guards m_Records (written while recording, read by the device-lost report). */
        mutable std::mutex m_Mutex;

    public:
        /** Constructor. */
        explicit DiagnosticsModule(DiagnosticsDesc Desc = {});

        /** Destructor. */
        ~DiagnosticsModule() override;

        /** Non-copyable. */
        DiagnosticsModule(const DiagnosticsModule&) = delete;

        /** Non-copyable. */
        DiagnosticsModule& operator=(const DiagnosticsModule&) = delete;

        /** Adds the optional extensions and features selected in the description. */
        void ConfigureDevice(DeviceRequirements& Requirements) override;

        /** Registers the module with the Context and creates the breadcrumb storage. */
        void OnContextCreated(Context& Ctx) override;

        /** Releases the breadcrumb storage and unregisters the module. */
        void OnContextDestroying(Context& Ctx) override;

        /** True when VK_EXT_device_fault was enabled with deviceFault. */
        [[nodiscard]] static bool IsDeviceFaultAvailable(const Context& Ctx);

        /** True when robustBufferAccess2 is enabled on the device. */
        [[nodiscard]] static bool IsRobustBufferAccess2Enabled(const Context& Ctx);

        /** True when robustImageAccess2 is enabled on the device. */
        [[nodiscard]] static bool IsRobustImageAccess2Enabled(const Context& Ctx);

        /** True when nullDescriptor is enabled on the device. */
        [[nodiscard]] static bool IsNullDescriptorEnabled(const Context& Ctx);

        /** True when VK_AMD_buffer_marker or VK_NV_device_diagnostic_checkpoints is enabled on the device. */
        [[nodiscard]] static bool AreBreadcrumbsAvailable(const Context& Ctx);

        /** Options the module was created with. */
        [[nodiscard]] const DiagnosticsDesc& Desc() const noexcept { return m_Desc; }

        /** Breadcrumb mechanism in use (None until the Context is created, or when unsupported/disabled). */
        [[nodiscard]] BreadcrumbBackend Backend() const noexcept { return m_Backend; }

        /** True when breadcrumbs are written. */
        [[nodiscard]] bool HasBreadcrumbs() const noexcept { return m_Backend != BreadcrumbBackend::None; }

        /** Starts the breadcrumbs of one graph execution: forgets the pass names previously recorded for Slot/Stream. */
        void BeginBreadcrumbs(std::uint32_t Slot, BreadcrumbStream Stream, std::uint64_t Frame);

        /** Writes the begin (End = false) or end marker of pass PassIndex into Commands; Name is kept for the report. */
        void MarkPass(VkCommandBuffer Commands, std::uint32_t Slot, BreadcrumbStream Stream, std::uint64_t Frame, std::uint32_t PassIndex,
                      std::string_view Name, bool End);

        /** Describes the breadcrumbs of every recorded frame (Summary: the last pass that started and the unfinished ones). */
        [[nodiscard]] DeviceLostReport DescribeBreadcrumbs() const;

        /** Queries VK_EXT_device_fault (only meaningful after VK_ERROR_DEVICE_LOST); empty when unavailable. */
        [[nodiscard]] static DeviceLostReport DescribeDeviceFault(const Context& Ctx);

    private:
        /** Index of the record of Slot/Stream (or SIZE_MAX when out of range). */
        [[nodiscard]] std::size_t RecordIndex(std::uint32_t Slot, BreadcrumbStream Stream) const noexcept;

        /** Encoded marker value of one pass. */
        [[nodiscard]] static std::uint32_t MarkerValue(std::uint64_t Frame, std::uint32_t PassIndex) noexcept;

        /** Destroys the marker buffer. */
        void DestroyStorage() noexcept;
    };
}
