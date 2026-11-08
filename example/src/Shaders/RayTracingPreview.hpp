// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string_view>

namespace Shaders::RayTracingPreview
{
    /** Slang source for the ray tracing preview: a Cornell-box scene traced with hardware ray queries
     *  (primary + shadow + one reflection bounce) from a compute shader into a storage image. */
    inline constexpr std::string_view Shader = R"slang(
import luvk;

[[vk::binding(3, 0)]] RaytracingAccelerationStructure Scenes[];

struct PushData
{
    uint   Scene;         // bindless TLAS index
    uint   Image;         // bindless output image index
    float  Time;          // seconds
    float  Exposure;      // tone mapping exposure
    float4 LightPosition; // xyz = world position, w = unused
    float4 LightColor;    // rgb = color, a = intensity
    float4 Sphere;        // xyz = center, w = radius
    float4 Cube;          // xyz = center, w = rotation around Y
};

[[vk::push_constant]] PushData Push;

// Cornell box (open at the front) ---------------------------------------------------------
static const float3 BoxMin = float3(-1.0, 0.0, -2.0);
static const float3 BoxMax = float3(1.0, 2.0, 1.0);

float3 Sky(float3 Direction)
{
    const float3 Horizon = float3(0.22, 0.30, 0.42);
    const float3 Zenith  = float3(0.02, 0.05, 0.12);
    float3 Color = lerp(Horizon, Zenith, saturate(Direction.y * 0.9 + 0.2));

    // A soft sun in the same direction as the area light.
    const float3 SunDirection = normalize(Push.LightPosition.xyz - float3(0.0, 1.0, -0.5));
    const float  Sun = pow(saturate(dot(Direction, SunDirection)), 48.0);
    Color += float3(1.0, 0.82, 0.58) * Sun * 1.6;
    return Color;
}

float3 RotateY(float3 P, float Angle)
{
    const float S = sin(Angle);
    const float C = cos(Angle);
    return float3(C * P.x + S * P.z, P.y, -S * P.x + C * P.z);
}

float Checker(float3 P)
{
    const int X = int(floor((P.x - BoxMin.x) * 4.0));
    const int Z = int(floor((P.z - BoxMin.z) * 3.0));
    return ((X + Z) & 1) == 0 ? 0.72 : 0.14;
}

void Surface(uint Instance, float3 Position, out float3 Normal, out float3 Albedo, out float Reflectivity)
{
    Reflectivity = 0.05;
    Normal       = float3(0.0, 1.0, 0.0);
    Albedo       = float3(0.55, 0.55, 0.55);

    if (Instance == 0)
    {
        // Cornell box: identify the wall from the hit position (the box is axis aligned).
        if (Position.y < BoxMin.y + 0.001)
        {
            Normal       = float3(0.0, 1.0, 0.0);
            const float V = Checker(Position);
            Albedo       = float3(V * 0.95, V * 0.92, V * 0.85);
            Reflectivity = 0.18;
        }
        else if (Position.y > BoxMax.y - 0.001)
        {
            Normal = float3(0.0, -1.0, 0.0);
            Albedo = float3(0.78, 0.78, 0.74);
        }
        else if (Position.z < BoxMin.z + 0.001)
        {
            Normal = float3(0.0, 0.0, 1.0);
            Albedo = float3(0.82, 0.80, 0.76);
        }
        else if (Position.x < BoxMin.x + 0.001)
        {
            Normal = float3(1.0, 0.0, 0.0);
            Albedo = float3(0.10, 0.62, 0.22);
        }
        else
        {
            Normal = float3(-1.0, 0.0, 0.0);
            Albedo = float3(0.80, 0.12, 0.10);
        }
    }
    else if (Instance == 1)
    {
        // Rotated cube: rotate the hit into local space and use the dominant axis as the normal.
        const float3 Local    = RotateY(Position - Push.Cube.xyz, -Push.Cube.w);
        const float3 AbsLocal = abs(Local);
        float3       AxisNormal;
        if (AbsLocal.x >= AbsLocal.y && AbsLocal.x >= AbsLocal.z)
        {
            AxisNormal = float3(Local.x < 0.0 ? -1.0 : 1.0, 0.0, 0.0);
        }
        else if (AbsLocal.y >= AbsLocal.z)
        {
            AxisNormal = float3(0.0, Local.y < 0.0 ? -1.0 : 1.0, 0.0);
        }
        else
        {
            AxisNormal = float3(0.0, 0.0, Local.z < 0.0 ? -1.0 : 1.0);
        }

        Normal       = RotateY(AxisNormal, Push.Cube.w);
        Albedo       = float3(0.95, 0.36, 0.06);
        Reflectivity = 0.15;
    }
    else
    {
        Normal       = normalize(Position - Push.Sphere.xyz);
        Albedo       = float3(0.06, 0.55, 0.52);
        Reflectivity = 0.65;
    }
}

float3 LightSurface(float3 Position, float3 Normal, float3 Albedo)
{
    // Move the origin a little off the surface to avoid self intersection.
    const float3 Offset = Position + Normal * 0.002;

    const float3 ToLight  = Push.LightPosition.xyz - Offset;
    const float  Distance = length(ToLight);
    const float3 LightDir = ToLight / max(Distance, 1e-4);

    RayDesc ShadowRay;
    ShadowRay.Origin    = Offset;
    ShadowRay.Direction = LightDir;
    ShadowRay.TMin      = 0.001;
    ShadowRay.TMax      = Distance - 0.002;

    RayQuery<RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> Shadow;
    Shadow.TraceRayInline(Scenes[NonUniformResourceIndex(Push.Scene)],
                          RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_FORCE_OPAQUE,
                          0xFF,
                          ShadowRay);
    while (Shadow.Proceed())
    {
    }

    const bool  Occluded    = Shadow.CommittedStatus() == COMMITTED_TRIANGLE_HIT;
    const float NdotL       = saturate(dot(Normal, LightDir));
    const float Attenuation = 1.0 / (1.0 + Distance * Distance * 0.15);
    const float3 Ambient    = float3(0.10, 0.12, 0.16);
    const float3 Direct     = Occluded ? float3(0.0) : Push.LightColor.rgb * Push.LightColor.a * NdotL * Attenuation;
    return Albedo * (Ambient + Direct);
}

/** Shades one ray without spawning reflections; used for the single reflection bounce. */
float3 ShadeOnce(RayDesc Ray)
{
    RayQuery<RAY_FLAG_FORCE_OPAQUE> Query;
    Query.TraceRayInline(Scenes[NonUniformResourceIndex(Push.Scene)], RAY_FLAG_NONE, 0xFF, Ray);
    while (Query.Proceed())
    {
    }

    if (Query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
    {
        return Sky(Ray.Direction);
    }

    const float3 Position = Ray.Origin + Ray.Direction * Query.CommittedRayT();
    float3       Normal;
    float3       Albedo;
    float        Reflectivity;
    Surface(Query.CommittedInstanceID(), Position, Normal, Albedo, Reflectivity);
    return LightSurface(Position, Normal, Albedo);
}

float3 TraceColor(RayDesc Ray)
{
    RayQuery<RAY_FLAG_FORCE_OPAQUE> Query;
    Query.TraceRayInline(Scenes[NonUniformResourceIndex(Push.Scene)], RAY_FLAG_NONE, 0xFF, Ray);
    while (Query.Proceed())
    {
    }

    if (Query.CommittedStatus() != COMMITTED_TRIANGLE_HIT)
    {
        return Sky(Ray.Direction);
    }

    const float3 Position = Ray.Origin + Ray.Direction * Query.CommittedRayT();
    float3       Normal;
    float3       Albedo;
    float        Reflectivity;
    Surface(Query.CommittedInstanceID(), Position, Normal, Albedo, Reflectivity);

    float3 Color = LightSurface(Position, Normal, Albedo);

    // One reflection bounce (no further recursion).
    if (Reflectivity > 0.01)
    {
        RayDesc ReflectionRay;
        ReflectionRay.Origin    = Position + Normal * 0.002;
        ReflectionRay.Direction = reflect(Ray.Direction, Normal);
        ReflectionRay.TMin      = 0.001;
        ReflectionRay.TMax      = 100.0;
        const float3 Reflected  = ShadeOnce(ReflectionRay);
        Color = lerp(Color, Reflected * 0.85 + Albedo * 0.15, Reflectivity);
    }

    return Color;
}

[shader("compute")]
[numthreads(8, 8, 1)]
void ComputeMain(uint3 Id : SV_DispatchThreadID)
{
    uint Width, Height;
    LuvkImages[Push.Image].GetDimensions(Width, Height);
    if (Id.x >= Width || Id.y >= Height)
    {
        return;
    }

    const float2 Uv  = (float2(Id.xy) + 0.5) / float2(Width, Height);
    const float2 Ndc = float2(Uv.x * 2.0 - 1.0, 1.0 - Uv.y * 2.0);

    // Camera inside the Cornell box, just behind the open front, looking towards the back wall.
    const float  Angle   = Push.Time * 0.28 + 0.15;
    const float3 Target  = float3(0.0, 0.95, -1.35);
    const float3 Origin  = float3(sin(Angle) * 0.62, 1.12 + 0.04 * sin(Angle * 2.0), 0.72);
    const float3 Forward = normalize(Target - Origin);
    const float3 Right   = normalize(cross(float3(0.0, 1.0, 0.0), Forward));
    const float3 Up      = cross(Forward, Right);

    RayDesc Ray;
    Ray.Origin    = Origin;
    Ray.Direction = normalize(Forward + Ndc.x * 0.72 * Right + Ndc.y * 0.72 * Up);
    Ray.TMin      = 0.001;
    Ray.TMax      = 100.0;

    float3 Color = TraceColor(Ray);

    // Exposure + gamma (the preview image is sampled as UNORM by Dear ImGui).
    Color = 1.0 - exp(-Color * Push.Exposure);
    Color = pow(saturate(Color), 1.0 / 2.2);

    LuvkImages[Push.Image][Id.xy] = float4(Color, 1.0);
}
)slang";
}
