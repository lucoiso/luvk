// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec2 InUv;

layout(location = 0) out vec4 OutColor;

layout(set = 0, binding = 0) uniform texture2D Textures[];
layout(set = 0, binding = 1) uniform sampler Samplers[];

layout(push_constant) uniform PushConstants
{
    uint TextureIndex;
    uint SamplerIndex;
    uint EncodeSrgb;
} Push;

vec3 EncodeSrgb(vec3 Linear)
{
    const vec3 Low = Linear * 12.92;
    const vec3 High = 1.055 * pow(max(Linear, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
    return mix(High, Low, step(Linear, vec3(0.0031308)));
}

void main()
{
    const vec3 Color = max(texture(sampler2D(Textures[nonuniformEXT(Push.TextureIndex)], Samplers[nonuniformEXT(Push.SamplerIndex)]), InUv).rgb, vec3(0.0));
    OutColor = vec4(Push.EncodeSrgb != 0u ? EncodeSrgb(Color) : Color, 1.0);
}
