// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 InPosition;
layout(location = 1) in vec2 InUv;
layout(location = 2) in vec4 InColor;

layout(location = 0) out vec4 OutColor;
layout(location = 1) out vec2 OutUv;

// Matches luvk::app::ImGuiPushConstants.
layout(push_constant) uniform PushConstants
{
    vec2 Scale;
    vec2 Translate;
    uint TextureIndex;
    uint SamplerIndex;
} Push;

void main()
{
    OutColor = InColor;
    OutUv = InUv;
    gl_Position = vec4(InPosition * Push.Scale + Push.Translate, 0.0, 1.0);
}
