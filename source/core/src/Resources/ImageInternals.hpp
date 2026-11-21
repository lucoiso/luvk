// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include "luvk/Core/Vulkan.hpp"
#include "luvk/Resources/Image.hpp"

namespace luvk
{
    /** Returns the aspect mask matching Value: color, depth, depth combined with stencil, or stencil. */
    [[nodiscard]] VkImageAspectFlags AspectFromFormat(Format Value) noexcept;

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
