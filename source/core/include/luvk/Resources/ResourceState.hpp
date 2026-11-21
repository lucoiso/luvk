// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>

namespace luvk
{
    /** Synchronisation state of a resource on the graphics timeline, used by the render graph to emit exactly the barriers
     *  that are needed (RAW, WAR, WAW and layout transitions). Values are the raw Vulkan enums and masks. */
    struct ResourceState
    {
        /** VkImageLayout the resource is currently in on the graphics timeline. */
        std::int32_t Layout = 0;

        /** VkPipelineStageFlags2 of the last write (or transition). */
        std::uint64_t WriteStages = 0U;

        /** VkAccessFlags2 of the last write to make available. */
        std::uint64_t WriteAccess = 0U;

        /** VkPipelineStageFlags2 reading since the last write (WAR hazard tracking). */
        std::uint64_t ReadStages = 0U;

        /** VkPipelineStageFlags2 that already see the last write. */
        std::uint64_t VisibleStages = 0U;

        /** VkAccessFlags2 that already see the last write. */
        std::uint64_t VisibleAccess = 0U;

        /** Last graphics timeline value of a frame that used the resource (uploads wait for it instead of all graphics work). */
        std::uint64_t LastGraphicsUse = 0U;

        /** Frame number of the last graph that imported the resource. */
        std::uint64_t ImportedInFrame = 0U;
    };
}
