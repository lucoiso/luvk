// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#version 460

layout(location = 0) in vec3 InPosition;
layout(location = 1) in vec4 InColor;

layout(location = 0) out vec4 OutColor;

// Matches luvk::app::DebugDrawPushConstants.
layout(push_constant) uniform PushConstants
{
    mat4 ViewProjection;
} Push;

void main()
{
    OutColor = InColor;
    gl_Position = Push.ViewProjection * vec4(InPosition, 1.0);
}
