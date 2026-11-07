// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Core/Common.hpp"

namespace luvk
{
    /** Synchronisation state of a resource on the graphics timeline, used by the render graph to emit exactly the barriers
     *  that are needed (RAW, WAR, WAW and layout transitions). */
    struct ResourceState
    {
        /** Image layout the resource is currently in on the graphics timeline. */
        VkImageLayout Layout = VK_IMAGE_LAYOUT_UNDEFINED;

        /** Stage masks of the last write (or transition). */
        VkPipelineStageFlags2 WriteStages = VK_PIPELINE_STAGE_2_NONE;

        /** Access masks of the last write to make available. */
        VkAccessFlags2 WriteAccess = VK_ACCESS_2_NONE;

        /** Stage masks reading since the last write (WAR hazard tracking). */
        VkPipelineStageFlags2 ReadStages = VK_PIPELINE_STAGE_2_NONE;

        /** Stage masks that already see the last write. */
        VkPipelineStageFlags2 VisibleStages = VK_PIPELINE_STAGE_2_NONE;

        /** Access masks that already see the last write. */
        VkAccessFlags2 VisibleAccess = VK_ACCESS_2_NONE;
    };
}
