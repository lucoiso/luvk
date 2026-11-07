// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#version 460
#extension GL_EXT_buffer_reference : require

// luvk::scene::Vertex: position, normal and texture coordinate as 8 tightly packed floats.
layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer VertexData
{
    float Values[];
};

// One model matrix per instance of the batch.
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer InstanceData
{
    mat4 Models[];
};

// Matches luvk::scene::MeshRenderer's FrameConstants.
layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer FrameData
{
    mat4 ViewProjection;
    vec4 CameraPosition;
    vec4 LightDirection;
    vec4 LightColor;
};

// Matches luvk::scene::MeshRenderer's MeshPushConstants.
layout(push_constant) uniform PushConstants
{
    FrameData    Frame;
    VertexData   Vertices;
    InstanceData Instances;
    uint         Texture;
    uint         Sampler;
    vec4         BaseColor;
    vec4         Emissive;
    uint         Flags;
} Push;

layout(location = 0) out vec3 OutWorldPosition;
layout(location = 1) out vec3 OutNormal;
layout(location = 2) out vec2 OutTexCoord;

void main()
{
    const uint Base     = uint(gl_VertexIndex) * 8u;
    const vec3 Position = vec3(Push.Vertices.Values[Base + 0u], Push.Vertices.Values[Base + 1u], Push.Vertices.Values[Base + 2u]);
    const vec3 Normal   = vec3(Push.Vertices.Values[Base + 3u], Push.Vertices.Values[Base + 4u], Push.Vertices.Values[Base + 5u]);
    const vec2 TexCoord = vec2(Push.Vertices.Values[Base + 6u], Push.Vertices.Values[Base + 7u]);

    const mat4 Model = Push.Instances.Models[gl_InstanceIndex];
    const vec4 World = Model * vec4(Position, 1.0);

    OutWorldPosition = World.xyz;
    OutNormal        = transpose(inverse(mat3(Model))) * Normal;
    OutTexCoord      = TexCoord;
    gl_Position      = Push.Frame.ViewProjection * World;
}
