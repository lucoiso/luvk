// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <type_traits>
#include "luvk/Core/Types.hpp"

#ifndef LUVK_API
#    define LUVK_API
#endif

namespace luvk
{
    /** Converts any Vulkan handle (dispatchable or not, 32 or 64 bit builds) to a 64-bit integer. */
    template <typename HandleType>
    [[nodiscard]] std::uint64_t HandleToU64(const HandleType Handle) noexcept
    {
        if constexpr (std::is_pointer_v<HandleType>)
        {
            return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(Handle));
        }
        else
        {
            return static_cast<std::uint64_t>(Handle);
        }
    }

    /** Inverse of HandleToU64. */
    template <typename HandleType>
    [[nodiscard]] HandleType HandleFromU64(const std::uint64_t Value) noexcept
    {
        if constexpr (std::is_pointer_v<HandleType>)
        {
            return reinterpret_cast<HandleType>(static_cast<std::uintptr_t>(Value));
        }
        else
        {
            return static_cast<HandleType>(Value);
        }
    }

    /** Size of the push constant block shared by every luvk pipeline: 128 bytes, the portable minimum every device
     *  guarantees (maxPushConstantsSize >= 128). Larger data goes through buffer device addresses. */
    inline constexpr std::uint32_t PushConstantSize = 128U;

    /** Sentinel for "no bindless slot". */
    inline constexpr std::uint32_t InvalidBindlessIndex = 0xFFFFFFFFU;

    /** Selects every mip level / array layer of an image (see Image::Subview and TextureHandle::Mip / Layer). */
    inline constexpr std::uint32_t AllSubresources = 0xFFFFFFFFU;
}
