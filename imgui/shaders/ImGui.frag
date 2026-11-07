// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#version 460
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec4 InColor;
layout(location = 1) in vec2 InUv;

layout(location = 0) out vec4 OutColor;

// The global bindless heap (set 0): sampled images and samplers indexed through push constants.
layout(set = 0, binding = 0) uniform texture2D Textures[];
layout(set = 0, binding = 1) uniform sampler Samplers[];

layout(push_constant) uniform PushConstants
{
    vec2 Scale;
    vec2 Translate;
    uint TextureIndex;
    uint SamplerIndex;
} Push;

void main()
{
    const vec4 Texel = texture(sampler2D(Textures[nonuniformEXT(Push.TextureIndex)], Samplers[nonuniformEXT(Push.SamplerIndex)]), InUv);
    OutColor = InColor * Texel;
}
