// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

// Fullscreen composite of SceneColor into the backbuffer: one triangle, optional sRGB encoding for UNORM targets.

import luvk;

struct CompositePush
{
    luvk::Texture2D     Source;
    luvk::SamplerHandle Sampler;
    uint                EncodeSrgb;
};

[[vk::push_constant]] ConstantBuffer<CompositePush> Push;

struct CompositeVaryings
{
    float4 Position : SV_Position;
    float2 Uv : TEXCOORD0;
};

float3 EncodeSrgb(float3 Linear)
{
    const float3 Low  = Linear * 12.92;
    const float3 High = 1.055 * pow(max(Linear, 0.0), 1.0 / 2.4) - 0.055;
    return select(Linear <= 0.0031308, Low, High);
}

[shader("vertex")]
CompositeVaryings CompositeVertex(uint VertexId : SV_VulkanVertexID)
{
    const float2 Uv = float2(float((VertexId << 1) & 2), float(VertexId & 2));

    CompositeVaryings Output;
    Output.Uv       = Uv;
    Output.Position = float4(Uv * 2.0 - 1.0, 0.0, 1.0);
    return Output;
}

[shader("fragment")]
float4 CompositeFragment(CompositeVaryings Input) : SV_Target0
{
    const float3 Color = max(Push.Source.Sample(Push.Sampler, Input.Uv).rgb, 0.0);
    return float4(Push.EncodeSrgb != 0 ? EncodeSrgb(Color) : Color, 1.0);
}
