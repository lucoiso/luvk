/*
 * Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#pragma once

#include <array>
#include <volk.h>

namespace luvk
{
    struct LUVK_API RenderTarget
    {
        VkRenderPass                 RenderPass;
        VkFramebuffer                Framebuffer;
        VkExtent2D                   Extent;
        std::array<VkClearValue, 2U> ClearValues{VkClearValue{.color = {0.2F,
                                                                        0.2F,
                                                                        0.2F,
                                                                        1.F}},
                                                 VkClearValue{.depthStencil = {1.F,
                                                                               0U}}};
    };
}
