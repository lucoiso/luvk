// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string_view>

namespace Shaders::Skybox
{
    /** Slang source for the skybox example: a fullscreen triangle reconstructing a view ray per pixel and sampling the procedural cubemap. */
    inline constexpr std::string_view Shader = R"slang(
import luvk;

struct PushData
{
    float4x4 InverseViewProjection;
    float3   CameraPosition;
    uint     Texture;
    uint     Sampler;
};

[[vk::push_constant]] PushData Push;

struct VertexOutput
{
    float4 Position : SV_Position;
    float2 Ndc      : TEXCOORD0;
};

[shader("vertex")]
VertexOutput VertexMain(uint VertexId : SV_VertexID)
{
    const float2 UV = float2((VertexId << 1) & 2, VertexId & 2);

    VertexOutput Output;
    Output.Ndc      = UV * 2.0 - 1.0;
    Output.Position = float4(Output.Ndc, 0.0, 1.0);
    return Output;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput Input) : SV_Target
{
    const float4 Far       = mul(Push.InverseViewProjection, float4(Input.Ndc, 1.0, 1.0));
    const float3 Direction = normalize(Far.xyz / Far.w - Push.CameraPosition);
    return float4(luvk::SampleCube(Push.Texture, Push.Sampler, Direction).rgb, 1.0);
}
)slang";
}
