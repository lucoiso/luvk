// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/App/Camera.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include "luvk/Platform/Input.hpp"

void luvk::app::Camera::Update(const platform::Input& State, const float DeltaTime)
{
    if (State.RightDown())
    {
        const glm::vec2 Delta = State.MouseDelta() * Sensitivity;
        Yaw += Delta.x;
        Pitch = std::clamp(Pitch - Delta.y, -89.F, 89.F);
    }

    if (State.Wheel() != 0.F)
    {
        Speed = std::clamp(Speed * (State.Wheel() > 0.F ? 1.2F : 1.F / 1.2F), 0.1F, 200.F);
    }

    const glm::vec3 Front = Forward();
    const glm::vec3 Right = glm::normalize(glm::cross(Front, glm::vec3{0.F, 1.F, 0.F}));
    glm::vec3 Move(0.F);

    if (State.IsKeyDown(SDLK_W) || State.IsKeyDown(SDLK_UP))
    {
        Move += Front;
    }
    if (State.IsKeyDown(SDLK_S) || State.IsKeyDown(SDLK_DOWN))
    {
        Move -= Front;
    }
    if (State.IsKeyDown(SDLK_D) || State.IsKeyDown(SDLK_RIGHT))
    {
        Move += Right;
    }
    if (State.IsKeyDown(SDLK_A) || State.IsKeyDown(SDLK_LEFT))
    {
        Move -= Right;
    }
    if (State.IsKeyDown(SDLK_E))
    {
        Move.y += 1.F;
    }
    if (State.IsKeyDown(SDLK_Q))
    {
        Move.y -= 1.F;
    }

    if (glm::dot(Move, Move) > 0.F)
    {
        const float Boost = State.IsKeyDown(SDLK_LSHIFT) ? 4.F : 1.F;
        Position += glm::normalize(Move) * Speed * Boost * DeltaTime;
    }
}

glm::vec3 luvk::app::Camera::Forward() const
{
    const float YawRadians   = glm::radians(Yaw);
    const float PitchRadians = glm::radians(Pitch);
    return glm::normalize(glm::vec3{std::cos(YawRadians) * std::cos(PitchRadians), std::sin(PitchRadians), std::sin(YawRadians) * std::cos(PitchRadians)});
}

glm::mat4 luvk::app::Camera::ViewMatrix() const
{
    return glm::lookAt(Position, Position + Forward(), glm::vec3{0.F, 1.F, 0.F});
}

glm::mat4 luvk::app::Camera::ProjectionMatrix(const float AspectRatio) const
{
    // Swapping the clip distances of a [0, 1] projection maps near to 1 and far to 0.
    glm::mat4 Projection = ReverseDepth ? glm::perspective(glm::radians(FieldOfView), AspectRatio, Far, Near)
                                        : glm::perspective(glm::radians(FieldOfView), AspectRatio, Near, Far);

    Projection[1][1] *= -1.F;
    return Projection;
}

luvk::frame::ViewData luvk::app::Camera::MakeView(const float AspectRatio) const
{
    frame::ViewData Data{.View = ViewMatrix(), .Projection = ProjectionMatrix(AspectRatio), .Position = Position, .Near = Near, .Far = Far, .ReverseDepth = ReverseDepth};
    Data.ViewProjection        = Data.Projection * Data.View;
    Data.InverseViewProjection = glm::inverse(Data.ViewProjection);
    return Data;
}
