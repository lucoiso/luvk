/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <vector>
#include <volk.h>

namespace luvk
{
    struct LUVK_API FrameData
    {
        bool                         Submitted{false};
        VkFence                      InFlight{VK_NULL_HANDLE};
        VkSemaphore                  ImageAvailable{VK_NULL_HANDLE};
        VkCommandBuffer              CommandBuffer{VK_NULL_HANDLE};
        std::vector<VkCommandBuffer> SecondaryBuffers{};
    };
}
