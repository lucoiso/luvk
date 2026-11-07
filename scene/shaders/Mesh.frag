// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_nonuniform_qualifier : require

layout(buffer_reference, std430, buffer_reference_align = 4) readonly buffer VertexData
{
    float Values[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer InstanceData
{
    mat4 Models[];
};

layout(buffer_reference, std430, buffer_reference_align = 16) readonly buffer FrameData
{
    mat4 ViewProjection;
    vec4 CameraPosition;
    vec4 LightDirection;
    vec4 LightColor;
};

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

layout(set = 0, binding = 0) uniform texture2D Textures[];
layout(set = 0, binding = 1) uniform sampler Samplers[];

layout(location = 0) in vec3 InWorldPosition;
layout(location = 1) in vec3 InNormal;
layout(location = 2) in vec2 InTexCoord;

layout(location = 0) out vec4 OutColor;

// Matches luvk::scene::MeshRenderer's material flags.
const uint FlagLit     = 1u;
const uint FlagTexture = 2u;

void main()
{
    vec4 Albedo = Push.BaseColor;
    if ((Push.Flags & FlagTexture) != 0u)
    {
        Albedo *= texture(sampler2D(Textures[nonuniformEXT(Push.Texture)], Samplers[nonuniformEXT(Push.Sampler)]), InTexCoord);
    }

    vec3 Color = Albedo.rgb;
    if ((Push.Flags & FlagLit) != 0u)
    {
        // Lambert + Blinn-Phong under one directional light; Emissive.w is the roughness.
        const vec3  Normal    = normalize(gl_FrontFacing ? InNormal : -InNormal);
        const vec3  ToLight   = normalize(-Push.Frame.LightDirection.xyz);
        const vec3  ToCamera  = normalize(Push.Frame.CameraPosition.xyz - InWorldPosition);
        const vec3  Halfway   = normalize(ToLight + ToCamera);
        const float Shininess = mix(256.0, 4.0, clamp(Push.Emissive.w, 0.0, 1.0));
        const float Diffuse   = max(dot(Normal, ToLight), 0.0);
        const float Specular  = Diffuse > 0.0 ? pow(max(dot(Normal, Halfway), 0.0), Shininess) * (1.0 - Push.Emissive.w) : 0.0;
        const float Ambient   = Push.Frame.LightDirection.w;

        Color = Albedo.rgb * (Ambient + Diffuse * Push.Frame.LightColor.rgb) + Specular * Push.Frame.LightColor.rgb;
    }

    OutColor = vec4(Color + Push.Emissive.rgb, Albedo.a);
}
