// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "ErrorInternals.hpp"
#include "luvk/Core/Error.hpp"
#include <atomic>
#include <string>
#include "luvk/Core/Context.hpp"

namespace
{
    /** Context receiving device-lost notifications (see SetDeviceLostObserver). */
    std::atomic<luvk::Context*> g_DeviceLostObserver = nullptr;

    std::string BuildMessage(const std::int32_t Result, const std::string_view What, const std::source_location& Location)
    {
        std::string Message;
        Message.reserve(256);
        Message += "[luvk] ";
        Message += What;
        Message += " failed with ";
        Message += luvk::ResultName(Result);
        Message += " (";
        Message += std::to_string(Result);
        Message += ") at ";
        Message += Location.file_name();
        Message += ':';
        Message += std::to_string(Location.line());
        return Message;
    }
}

void luvk::SetDeviceLostObserver(Context* const Observer) noexcept
{
    g_DeviceLostObserver.store(Observer, std::memory_order_release);
}

void luvk::Check(const std::int32_t Result, const std::string_view What, const std::source_location& Location)
{
    if (Result < 0)
    {
        if (Result == VK_ERROR_DEVICE_LOST)
        {
            // The report is produced before the exception propagates, while the breadcrumbs are still intact.
            if (Context* const Observer = g_DeviceLostObserver.load(std::memory_order_acquire))
            {
                Observer->ReportDeviceLost(std::string(What) + " at " + Location.file_name() + ':' + std::to_string(Location.line()));
            }
        }
        throw VulkanError(Result, What, Location);
    }
}

luvk::VulkanError::VulkanError(const std::int32_t Result, const std::string_view What, const std::source_location& Location)
    : std::runtime_error(BuildMessage(Result, What, Location)),
      m_Result(Result) {}

const char* luvk::ResultName(const std::int32_t Result) noexcept
{
    switch (static_cast<VkResult>(Result))
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
    default: return "VkResult(unknown)";
    }
}
