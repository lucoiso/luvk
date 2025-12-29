/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <cstdint>

namespace luvk::Constants
{
    /**
     * Maximum supported frames in flight for static array sizing.
     * Actual usage (Double vs Triple buffering) is determined at runtime.
     */
    constexpr std::uint32_t MaxFramesInFlight = 3U;
}
