// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

// Debug lines. CPU lines are pulled from the frame allocator; GPU lines from the buffer compute shaders append to
// (GpuDebugLinesHeader followed by the vertices). Both use the 16-byte vertex: position and packed RGBA8 color.

import luvk;

struct LineVertex
{
    float3 Position;
    uint   Color;
};

struct DebugLinesHeader
{
    uint VertexCount;
    uint InstanceCount;
    uint FirstVertex;
    uint FirstInstance;
    uint Capacity;
    uint Padding0;
    uint Padding1;
    uint Padding2;
};

struct DebugDrawPush
{
    float4x4                  ViewProjection;
    luvk::Span<LineVertex>    Vertices;
};

struct DebugDrawGpuPush
{
    float4x4          ViewProjection;
    DebugLinesHeader* Lines;
};

[[vk::push_constant]] ConstantBuffer<DebugDrawPush> Push;
[[vk::push_constant]] ConstantBuffer<DebugDrawGpuPush> GpuPush;

struct LineVaryings
{
    float4 Position : SV_Position;
    float4 Color : COLOR0;
};

float4 UnpackColor(uint Packed)
{
    return float4(float(Packed & 0xFF), float((Packed >> 8) & 0xFF), float((Packed >> 16) & 0xFF), float(Packed >> 24)) / 255.0;
}

LineVaryings Emit(float4x4 ViewProjection, LineVertex Vertex)
{
    LineVaryings Output;
    Output.Color    = UnpackColor(Vertex.Color);
    Output.Position = mul(ViewProjection, float4(Vertex.Position, 1.0));
    return Output;
}

[shader("vertex")]
LineVaryings DebugDrawVertex(uint VertexId : SV_VulkanVertexID)
{
    return Emit(Push.ViewProjection, Push.Vertices.Load(VertexId));
}

[shader("vertex")]
LineVaryings DebugDrawGpuVertex(uint VertexId : SV_VulkanVertexID)
{
    // Producers may reserve past the capacity: those vertices are dropped (moved outside the clip volume).
    if (VertexId >= GpuPush.Lines->Capacity)
    {
        LineVaryings Dropped;
        Dropped.Color    = 0.0;
        Dropped.Position = float4(2.0, 2.0, 2.0, 1.0);
        return Dropped;
    }

    let Vertices = (LineVertex*)(GpuPush.Lines + 1);
    return Emit(GpuPush.ViewProjection, Vertices[VertexId]);
}

[shader("fragment")]
float4 DebugDrawFragment(LineVaryings Input) : SV_Target0
{
    return Input.Color;
}
