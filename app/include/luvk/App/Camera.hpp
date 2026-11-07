// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <glm/glm.hpp>
#include "luvk/App/Scene.hpp"

namespace luvk::platform
{
    class Input;
}

namespace luvk::app
{
    /** Fly camera: WASD / arrows move, Q/E down/up, right mouse button looks around, Shift is faster.
     *  The mouse wheel changes the speed. Uses Vulkan conventions (Y down in clip space, depth in [0, 1]). */
    class Camera
    {
    public:
        /** Camera world-space position, in world units. */
        glm::vec3 Position{0.F, 0.F, 3.F};

        /** Degrees; -90 looks towards -Z. */
        float Yaw = -90.F;

        /** Degrees. */
        float Pitch = 0.F;

        /** Vertical field of view in degrees. */
        float FieldOfView = 60.F;

        /** Near clip distance in world units. */
        float Near = 0.05F;

        /** Far clip distance in world units. */
        float Far = 500.F;

        /** Movement speed in world units per second. */
        float Speed = 3.F;

        /** Mouse look sensitivity in degrees per pixel of mouse delta. */
        float Sensitivity = 0.15F;

        /** Reversed depth projection (near = 1, far = 0); the Application keeps it in sync with FrameSettings::ReverseDepth. */
        bool ReverseDepth = false;

        /** Applies look/movement input for DeltaTime seconds, updating position, yaw, pitch and speed. */
        void Update(const platform::Input& State, float DeltaTime);

        /** Returns the normalized forward direction derived from yaw and pitch. */
        [[nodiscard]] glm::vec3 Forward() const;

        /** Returns the look-at view matrix for the current position and orientation. */
        [[nodiscard]] glm::mat4 ViewMatrix() const;

        /** Returns the perspective projection for AspectRatio, flipped in Y for Vulkan clip space. */
        [[nodiscard]] glm::mat4 ProjectionMatrix(float AspectRatio) const;

        /** Builds a ViewData with the view/projection matrices, position and clip distances. */
        [[nodiscard]] ViewData MakeView(float AspectRatio) const;
    };
}
