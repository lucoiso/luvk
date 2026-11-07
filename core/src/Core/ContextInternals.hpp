// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string>
#include <vector>
#include "luvk/Core/Context.hpp"

namespace luvk
{
    /** One physical device evaluated during selection (scored and compared against the requirements). */
    struct Candidate
    {
        /** Handle of the physical device being evaluated. */
        VkPhysicalDevice Device = VK_NULL_HANDLE;

        /** Highest Vulkan API version supported by the device (packed VK_VERSION numbering). */
        std::uint32_t ApiVersion = 0U;

        /** Device extensions available for requirement checks. */
        std::vector<std::string> Extensions;

        /** Feature and property chain enabled for the candidate. */
        FeatureChain Enabled = {};

        /** Queue family index used for graphics and presentation. */
        std::uint32_t GraphicsFamily = 0U;

        /** Queue family index used for transfer work (dedicated copy family when one exists). */
        std::uint32_t TransferFamily = 0U;

        /** Queue index within TransferFamily used to create the transfer queue. */
        std::uint32_t TransferIndex = 0U;

        /** Selection score of the device (negative when it does not meet the requirements). */
        std::int64_t Score = -1;
    };
}
