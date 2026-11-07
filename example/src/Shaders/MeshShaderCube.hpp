// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string_view>

namespace Shaders::MeshShaderCube
{
    /** Slang source for the mesh shader cube example: one task workgroup amplified into six mesh workgroups (one per cube face). */
    inline constexpr std::string_view Shader = R"slang(
struct PushData
{
    float4x4 ModelViewProjection;
};

[[vk::push_constant]] PushData Push;

struct Payload
{
    uint Unused;
};

groupshared Payload SharedPayload;

// One task workgroup amplifies into six mesh workgroups: one per cube face.
[shader("amplification")]
[numthreads(1, 1, 1)]
void TaskMain()
{
    DispatchMesh(6, 1, 1, SharedPayload);
}

struct MeshVertex
{
    float4 Position : SV_Position;
    float3 Color    : COLOR;
};

static const float3 Corners[8] = {
    float3(-0.5, -0.5, -0.5), float3(0.5, -0.5, -0.5), float3(0.5, 0.5, -0.5), float3(-0.5, 0.5, -0.5),
    float3(-0.5, -0.5,  0.5), float3(0.5, -0.5,  0.5), float3(0.5, 0.5,  0.5), float3(-0.5, 0.5,  0.5)
};

static const uint4 Faces[6] = {
    uint4(4, 5, 6, 7), uint4(1, 0, 3, 2), uint4(5, 1, 2, 6),
    uint4(0, 4, 7, 3), uint4(3, 7, 6, 2), uint4(0, 1, 5, 4)
};

static const float3 FaceColors[6] = {
    float3(1.0, 0.25, 0.25), float3(0.25, 1.0, 0.25), float3(0.25, 0.45, 1.0),
    float3(1.0, 1.0, 0.25),  float3(1.0, 0.25, 1.0),  float3(0.25, 1.0, 1.0)
};

[shader("mesh")]
[outputtopology("triangle")]
[numthreads(4, 1, 1)]
void MeshMain(uint3 Group : SV_GroupID, uint Thread : SV_GroupThreadID,
              OutputVertices<MeshVertex, 4> Vertices, OutputIndices<uint3, 2> Triangles)
{
    SetMeshOutputCounts(4, 2);

    const uint Face = Group.x;
    Vertices[Thread].Position = mul(Push.ModelViewProjection, float4(Corners[Faces[Face][Thread]], 1.0));
    Vertices[Thread].Color    = FaceColors[Face];

    if (Thread < 2)
    {
        Triangles[Thread] = Thread == 0 ? uint3(0, 1, 2) : uint3(0, 2, 3);
    }
}

[shader("fragment")]
float4 FragmentMain(MeshVertex Input) : SV_Target
{
    return float4(Input.Color, 1.0);
}
)slang";
}
