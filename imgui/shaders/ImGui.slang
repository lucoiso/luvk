// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

// Dear ImGui through luvk: vertices are pulled from the frame allocator (ImDrawVert: position, uv, packed RGBA8 color,
// 20 bytes read as five 32-bit words), textures and samplers are bindless handles.

import luvk;

struct ImGuiPush
{
    float2              Scale;
    float2              Translate;
    uint*               Vertices;
    luvk::Texture2D     Texture;
    luvk::SamplerHandle Sampler;
};

[[vk::push_constant]] ConstantBuffer<ImGuiPush> Push;

struct ImGuiVaryings
{
    float4 Position : SV_Position;
    float4 Color : COLOR0;
    float2 Uv : TEXCOORD0;
};

[shader("vertex")]
ImGuiVaryings ImGuiVertex(uint VertexId : SV_VulkanVertexID)
{
    let Vertex = Push.Vertices + VertexId * 5;
    const uint   Packed = Vertex[4];
    const float2 Position = float2(asfloat(Vertex[0]), asfloat(Vertex[1]));

    ImGuiVaryings Output;
    Output.Uv       = float2(asfloat(Vertex[2]), asfloat(Vertex[3]));
    Output.Color    = float4(float(Packed & 0xFF), float((Packed >> 8) & 0xFF), float((Packed >> 16) & 0xFF), float(Packed >> 24)) / 255.0;
    Output.Position = float4(Position * Push.Scale + Push.Translate, 0.0, 1.0);
    return Output;
}

[shader("fragment")]
float4 ImGuiFragment(ImGuiVaryings Input) : SV_Target0
{
    return Input.Color * Push.Texture.Sample(Push.Sampler, Input.Uv);
}
