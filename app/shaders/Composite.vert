// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#version 460

layout(location = 0) out vec2 OutUv;

// Matches luvk::app::CompositePushConstants.
layout(push_constant) uniform PushConstants
{
    uint TextureIndex;
    uint SamplerIndex;
    uint EncodeSrgb;
} Push;

void main()
{
    const vec2 Uv = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    OutUv = Uv;
    gl_Position = vec4(Uv * 2.0 - 1.0, 0.0, 1.0);
}
