// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#version 460
#extension GL_EXT_buffer_reference : require

// Matches luvk::app::GpuDebugLineVertex (16 bytes).
struct LineVertex
{
    float X;
    float Y;
    float Z;
    uint  Color;
};

// Matches luvk::app::GpuDebugLinesHeader followed by the vertices. The first four words are the
// VkDrawIndirectCommand that DebugDraw draws with; GPU producers append with atomicAdd(VertexCount, 2).
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer DebugLines
{
    uint       VertexCount;
    uint       InstanceCount;
    uint       FirstVertex;
    uint       FirstInstance;
    uint       Capacity;
    uint       Padding0;
    uint       Padding1;
    uint       Padding2;
    LineVertex Vertices[];
};

layout(location = 0) out vec4 OutColor;

// Matches luvk::app::DebugDrawGpuPushConstants.
layout(push_constant) uniform PushConstants
{
    mat4       ViewProjection;
    DebugLines Lines;
} Push;

void main()
{
    const uint Index = uint(gl_VertexIndex);

    // Producers may reserve past the capacity: those vertices are dropped (moved outside the clip volume).
    if (Index >= Push.Lines.Capacity)
    {
        OutColor    = vec4(0.0);
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        return;
    }

    const LineVertex Vertex = Push.Lines.Vertices[Index];
    OutColor    = unpackUnorm4x8(Vertex.Color);
    gl_Position = Push.ViewProjection * vec4(Vertex.X, Vertex.Y, Vertex.Z, 1.0);
}
