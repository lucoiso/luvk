// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <source_location>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <vector>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    /** Returns the literal name of a VkResult (e.g. "VK_ERROR_DEVICE_LOST"), or "VkResult(unknown)". */
    [[nodiscard]] LUVK_API const char* ToString(VkResult Result) noexcept;

    /** Thrown whenever a Vulkan call fails. Carries the VkResult and the call site. */
    class LUVK_API VulkanError : public std::runtime_error
    {
        /** VkResult code carried by the exception. */
        VkResult m_Result;

    public:
        /** Constructor: builds the message from the VkResult, the failed operation and the call site. */
        VulkanError(VkResult Result, std::string_view What, const std::source_location& Location = std::source_location::current());

        /** Returns the VkResult that caused the exception. */
        [[nodiscard]] VkResult Result() const noexcept
        {
            return m_Result;
        }
    };

    /** Throws VulkanError on any negative (error) VkResult. Positive codes (SUBOPTIMAL, INCOMPLETE...) are not errors. */
    LUVK_API void Check(VkResult Result, std::string_view What, const std::source_location& Location = std::source_location::current());

    /** Implements the Vulkan "two-call" enumeration idiom, handling VK_INCOMPLETE. */
    template <typename T, typename Function, typename... Arguments>
    [[nodiscard]] std::vector<T> Enumerate(Function&& Func, Arguments... Args)
    {
        std::vector<T> Output;
        std::uint32_t  Count = 0U;

        if constexpr (std::is_void_v<std::invoke_result_t<Function, Arguments..., std::uint32_t*, T*>>)
        {
            Func(Args..., &Count, nullptr);
            Output.resize(Count);
            Func(Args..., &Count, std::data(Output));
        }
        else
        {
            VkResult Result = VK_SUCCESS;
            do
            {
                Check(Func(Args..., &Count, nullptr), "enumeration (count)");
                Output.resize(Count);
                Result = Func(Args..., &Count, std::data(Output));
                Check(Result, "enumeration (data)");
            }
            while (Result == VK_INCOMPLETE);
        }

        Output.resize(Count);
        return Output;
    }
}
