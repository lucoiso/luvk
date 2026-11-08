// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

// MeshRenderer: vertex pulling (position, normal, uv as 8 floats), instanced transforms, unlit or lit materials (Lambert
// plus Blinn-Phong under one directional light; Emissive.w is the roughness).

import luvk;

struct FrameData
{
    float4x4 ViewProjection;
    float4   CameraPosition;
    float4   LightDirection;
    float4   LightColor;
};

struct MeshPush
{
    FrameData*          Frame;
    float*              Vertices;
    float4x4*           Instances;
    luvk::Texture2D     Texture;
    luvk::SamplerHandle Sampler;
    float4              BaseColor;
    float4              Emissive;
    uint                Flags;
};

[[vk::push_constant]] ConstantBuffer<MeshPush> Push;

static const uint FlagLit     = 1;
static const uint FlagTexture = 2;

struct MeshVaryings
{
    float4 Position : SV_Position;
    float3 WorldPosition : POSITION0;
    float3 Normal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
};

[shader("vertex")]
MeshVaryings MeshVertex(uint VertexId : SV_VulkanVertexID, uint InstanceId : SV_VulkanInstanceID)
{
    let Vertex = Push.Vertices + VertexId * 8;
    const float3 Position = float3(Vertex[0], Vertex[1], Vertex[2]);
    const float3 Normal   = float3(Vertex[3], Vertex[4], Vertex[5]);
    const float4x4 Model  = Push.Instances[InstanceId];
    const float4 World    = mul(Model, float4(Position, 1.0));

    // Normal matrix: the inverse transpose of the upper 3x3 is the cofactor matrix over the determinant; only the
    // determinant's sign matters once the fragment normalizes.
    const float3x3 Linear   = (float3x3)Model;
    const float3x3 Cofactor = float3x3(cross(Linear[1], Linear[2]), cross(Linear[2], Linear[0]), cross(Linear[0], Linear[1]));
    const float    Sign     = dot(Linear[0], Cofactor[0]) < 0.0 ? -1.0 : 1.0;

    MeshVaryings Output;
    Output.WorldPosition = World.xyz;
    Output.Normal        = mul(Cofactor, Normal) * Sign;
    Output.TexCoord      = float2(Vertex[6], Vertex[7]);
    Output.Position      = mul(Push.Frame->ViewProjection, World);
    return Output;
}

[shader("fragment")]
float4 MeshFragment(MeshVaryings Input, bool FrontFacing : SV_IsFrontFace) : SV_Target0
{
    float4 Albedo = Push.BaseColor;
    if ((Push.Flags & FlagTexture) != 0)
    {
        Albedo *= Push.Texture.Sample(Push.Sampler, Input.TexCoord);
    }

    float3 Color = Albedo.rgb;
    if ((Push.Flags & FlagLit) != 0)
    {
        const FrameData Frame     = *Push.Frame;
        const float3    Normal    = normalize(FrontFacing ? Input.Normal : -Input.Normal);
        const float3    ToLight   = normalize(-Frame.LightDirection.xyz);
        const float3    ToCamera  = normalize(Frame.CameraPosition.xyz - Input.WorldPosition);
        const float3    Halfway   = normalize(ToLight + ToCamera);
        const float     Shininess = lerp(256.0, 4.0, saturate(Push.Emissive.w));
        const float     Diffuse   = max(dot(Normal, ToLight), 0.0);
        const float     Specular  = Diffuse > 0.0 ? pow(max(dot(Normal, Halfway), 0.0), Shininess) * (1.0 - Push.Emissive.w) : 0.0;
        const float     Ambient   = Frame.LightDirection.w;
        Color = Albedo.rgb * (Ambient + Diffuse * Frame.LightColor.rgb) + Specular * Frame.LightColor.rgb;
    }

    return float4(Color + Push.Emissive.rgb, Albedo.a);
}
