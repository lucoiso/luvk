// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>
#include "luvk/Core/Error.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace luvk
{
    class Context;

    /**
     * Registers the Context told about every VK_ERROR_DEVICE_LOST seen by luvk::Check (nullptr unregisters). Only one
     * Context exists at a time, so a single process-wide observer is enough.
     */
    void SetDeviceLostObserver(Context* Observer) noexcept;

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
