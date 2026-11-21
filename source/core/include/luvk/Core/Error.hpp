// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    /** Returns the literal name of a VkResult value (e.g. "VK_ERROR_DEVICE_LOST"), or "VkResult(unknown)". */
    [[nodiscard]] LUVK_API const char* ResultName(std::int32_t Result) noexcept;

    /** Thrown whenever a Vulkan call fails. Carries the VkResult value and the call site. */
    class LUVK_API VulkanError : public std::runtime_error
    {
        /** VkResult value carried by the exception. */
        std::int32_t m_Result;

    public:
        /** Constructor: builds the message from the VkResult value, the failed operation and the call site. */
        VulkanError(std::int32_t Result, std::string_view What, const std::source_location& Location = std::source_location::current());

        /** Returns the VkResult value that caused the exception. */
        [[nodiscard]] std::int32_t Result() const noexcept
        {
            return m_Result;
        }
    };

    /** Throws VulkanError on any negative (error) VkResult value. Positive codes (SUBOPTIMAL, INCOMPLETE...) are not errors. */
    LUVK_API void Check(std::int32_t Result, std::string_view What, const std::source_location& Location = std::source_location::current());
}
