// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <array>
#include <atomic>
#include <string>
#include <vector>
#include "luvk/Core/Context.hpp"
#include "luvk/Core/FeatureChain.hpp"
#include "luvk/Core/Vulkan.hpp"

namespace luvk
{
    /** Vulkan state of the selected device, kept out of the public Context header. */
    struct DeviceInfo
    {
        /** Basic device properties (pNext chain set temporarily while querying). */
        VkPhysicalDeviceProperties2 Properties{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};

        /** Vulkan 1.1 core properties (subgroup size). */
        VkPhysicalDeviceVulkan11Properties Properties11{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_PROPERTIES};

        /** Vulkan 1.2 core properties. */
        VkPhysicalDeviceVulkan12Properties Properties12{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_PROPERTIES};

        /** Vulkan 1.3 core properties. */
        VkPhysicalDeviceVulkan13Properties Properties13{.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_PROPERTIES};

        /** Feature chain enabled at device creation. */
        FeatureChain EnabledFeatures = {};

        /** Identity and limits in luvk types. */
        DeviceLimits Limits = {};

        /** Usage each format supports with optimal tiling, resolved on first use (bit 31 marks a resolved entry). */
        mutable std::array<std::atomic<std::uint32_t>, 256> FormatUsage = {};
    };

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

        /** True when a dedicated compute queue (family without graphics) was assigned. */
        bool HasCompute = false;

        /** Queue family of the async compute queue. */
        std::uint32_t ComputeFamily = 0U;

        /** Queue index within ComputeFamily. */
        std::uint32_t ComputeIndex = 0U;

        /** True when an extra present queue was assigned. */
        bool HasPresent = false;

        /** Queue family of the extra present queue. */
        std::uint32_t PresentFamily = 0U;

        /** Queue index within PresentFamily. */
        std::uint32_t PresentIndex = 0U;

        /** True when an extra image-acquire queue was assigned. */
        bool HasImageAcquire = false;

        /** Queue family of the extra image-acquire queue. */
        std::uint32_t ImageAcquireFamily = 0U;

        /** Queue index within ImageAcquireFamily. */
        std::uint32_t ImageAcquireIndex = 0U;

        /** Selection score of the device (negative when it does not meet the requirements). */
        std::int64_t Score = -1;
    };
}
