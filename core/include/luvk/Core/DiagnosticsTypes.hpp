// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace luvk
{
    /** What DiagnosticsModule enables (every item is optional: unsupported ones are skipped, never fatal). */
    struct DiagnosticsDesc
    {
        /** VK_EXT_device_fault (deviceFault): the device-lost report then contains the driver's fault description,
         *  the faulting GPU addresses (with their type and precision) and the vendor fault codes. */
        bool DeviceFault = true;

        /** VK_EXT_robustness2 robustBufferAccess2 (also enables robustBufferAccess): out-of-bounds accesses through
         *  buffer descriptors are discarded / return zero instead of faulting. Costs GPU time; does not cover buffer
         *  device addresses (luvk's default way to reach buffers). */
        bool RobustBufferAccess2 = false;

        /** VK_EXT_robustness2 robustImageAccess2: out-of-bounds image accesses return zero / are discarded. */
        bool RobustImageAccess2 = false;

        /** VK_EXT_robustness2 nullDescriptor: nullptr descriptors are allowed and read as zero. */
        bool NullDescriptor = false;

        /** GPU breadcrumbs: the render graph writes a marker before and after every executed pass, through
         *  VK_AMD_buffer_marker (preferred) or VK_NV_device_diagnostic_checkpoints, so a device-lost report names the
         *  passes the GPU was executing. */
        bool Breadcrumbs = true;

        /** Frame slots with breadcrumb storage (frames in flight beyond this are not tracked). */
        std::uint32_t MaxFramesInFlight = 4U;

        /** Passes per frame and per graph with breadcrumb storage (later passes are not tracked). */
        std::uint32_t MaxPassesPerFrame = 1024U;
    };

    /** Mechanism used to write GPU breadcrumbs. */
    enum class BreadcrumbBackend : std::uint8_t
    {
        /** No breadcrumbs (disabled or unsupported). */
        None,

        /** vkCmdWriteBufferMarker2AMD / vkCmdWriteBufferMarkerAMD into a host-visible coherent buffer. */
        BufferMarkerAMD,

        /** vkCmdSetCheckpointNV, read back with vkGetQueueCheckpointData(2)NV. */
        CheckpointsNV,
    };

    /** Render graph whose passes a breadcrumb belongs to. */
    enum class BreadcrumbStream : std::uint8_t
    {
        /** The frame's main graph (graphics queue). */
        Graphics,

        /** The frame's async compute graph (compute queue, or the graphics queue without async compute). */
        AsyncCompute,
    };

    /** Human-readable description of a VK_ERROR_DEVICE_LOST, produced once by Context::ReportDeviceLost. */
    struct DeviceLostReport
    {
        /** A few lines for a message box: what failed, the GPU, the last pass that started and the fault description. */
        std::string Summary;

        /** The full report: device and driver, device fault information, breadcrumbs of the frames in flight. */
        std::string Details;

        /** File the report was written to (empty when ContextDesc::CrashReportDirectory was not set or writing failed). */
        std::filesystem::path File;
    };
}
