// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <glm/glm.hpp>
#include <luvk/Render/RenderGraph.hpp>
#include "luvk/Frame/Blackboard.hpp"

namespace luvk::frame
{
    /** Camera matrices published under Slots::View every frame. */
    struct ViewData
    {
        /** World-to-view (camera) matrix. */
        glm::mat4 View{1.F};

        /** View-to-clip projection matrix (Vulkan depth range [0, 1]). */
        glm::mat4 Projection{1.F};

        /** Combined Projection * View matrix. */
        glm::mat4 ViewProjection{1.F};

        /** Inverse of ViewProjection, mapping clip space back to world space. */
        glm::mat4 InverseViewProjection{1.F};

        /** Camera world-space position. */
        glm::vec3 Position{0.F};

        /** Near clip distance in world units. */
        float Near = 0.05F;

        /** Far clip distance in world units. */
        float Far = 500.F;

        /** True when Projection maps near to depth 1 and far to depth 0 (FrameSettings::ReverseDepth). */
        bool ReverseDepth = false;
    };

    /** Slots of the scene frame built by SceneRenderer, shared by every feature. */
    namespace Slots
    {
        /** Linear HDR scene color at render resolution (an upscaler may replace it with an output-resolution one). */
        inline constexpr SlotKey<TextureHandle> SceneColor{"SceneColor"};

        /** Scene depth at render resolution. */
        inline constexpr SlotKey<TextureHandle> SceneDepth{"SceneDepth"};

        /** Optional, written by features that need it (e.g. for temporal upscalers). */
        inline constexpr SlotKey<TextureHandle> MotionVectors{"MotionVectors"};

        /** Output image (output resolution), written by the Phases::Output features. */
        inline constexpr SlotKey<TextureHandle> Backbuffer{"Backbuffer"};

        /** GPU-written debug lines of the frame; drawn by the debug draw feature when present. */
        inline constexpr SlotKey<BufferHandle> DebugLines{"DebugLines"};

        /** Camera of the frame (only published when the application has one). */
        inline constexpr SlotKey<ViewData> View{"View"};
    }
}
