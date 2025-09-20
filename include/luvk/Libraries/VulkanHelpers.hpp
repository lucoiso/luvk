// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdio>
#include "luvk/Types/MeshEntry.hpp"

namespace luvk
{
    [[nodiscard]] constexpr const char* SeverityToString(const VkDebugUtilsMessageSeverityFlagBitsEXT Severity)
    {
        switch (Severity)
        {
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: return "Verbose";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: return "Info";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: return "Warning";
            case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: return "Error";
            default: return "Unknown";
        }
    }

    [[nodiscard]] constexpr const char* ResultToString(const VkResult ResultValue)
    {
        switch (ResultValue)
        {
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";
            case VK_EVENT_SET: return "VK_EVENT_SET";
            case VK_EVENT_RESET: return "VK_EVENT_RESET";
            case VK_INCOMPLETE: return "VK_INCOMPLETE";
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
            case VK_ERROR_UNKNOWN: return "VK_ERROR_UNKNOWN";
            case VK_ERROR_OUT_OF_POOL_MEMORY: return "VK_ERROR_OUT_OF_POOL_MEMORY";
            case VK_ERROR_INVALID_EXTERNAL_HANDLE: return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
            case VK_ERROR_FRAGMENTATION: return "VK_ERROR_FRAGMENTATION";
            case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS: return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
            case VK_PIPELINE_COMPILE_REQUIRED: return "VK_PIPELINE_COMPILE_REQUIRED";
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";
            case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
            default: return "Unhandled VkResult";
        }
    }

    template <typename Result>
    static bool ExecuteVulkanFunc(Result ResultValue, const char* Name, const char* File, const std::uint32_t Line)
    {
        if (ResultValue != VK_SUCCESS)
        {
            std::fprintf(stderr, "%s returned %s (%d) - %s:%d\n", Name, ResultToString(ResultValue), ResultValue, File, Line);
            return false;
        }
        return true;
    }
} // namespace luvk

#define LUVK_EXECUTE(expr) luvk::ExecuteVulkanFunc((expr), #expr, __FILE__, __LINE__)
