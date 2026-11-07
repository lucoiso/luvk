// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include "luvk/Resources/Image.hpp"

namespace luvk
{
    /** Texel block layout of one VkFormat (bytes per block and block extent). */
    struct FormatInfo
    {
        /** Size of one texel block in bytes. */
        std::uint32_t BlockBytes = 0U;

        /** Width of one texel block in texels. */
        std::uint32_t BlockWidth = 1U;

        /** Height of one texel block in texels. */
        std::uint32_t BlockHeight = 1U;
    };
}
