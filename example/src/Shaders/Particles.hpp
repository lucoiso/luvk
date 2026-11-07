// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string_view>

namespace Shaders::Particles
{
    /** Slang source for the particle fountain: a compute pass simulating particles in place through a buffer device
     *  address, then camera-facing quads pulled as instances and depth-tested against the scene. DebugMain optionally
     *  appends velocity lines to the frame's GPU debug line buffer. */
    inline constexpr std::string_view Shader = R"slang(
struct Particle
{
    float3 Position;
    float  Life;
    float3 Velocity;
    float  Seed;
};

struct PushData
{
    float4x4  ViewProjection;
    Particle* Particles;
    uint      Count;
    float     DeltaTime;
    float3    Emitter;
    float     Time;
    float3    CameraRight;
    float     Size;
    float3    CameraUp;
};

[[vk::push_constant]] PushData Push;

float Hash(float Value)
{
    return frac(sin(Value * 12.9898) * 43758.5453);
}

[shader("compute")]
[numthreads(64, 1, 1)]
void ComputeMain(uint3 Id : SV_DispatchThreadID)
{
    if (Id.x >= Push.Count)
    {
        return;
    }

    Particle Item = Push.Particles[Id.x];

    // A negative life is a spawn delay (the particle is invisible until it runs out).
    const bool Waiting = Item.Life < 0.0;
    Item.Life += Waiting ? Push.DeltaTime : -Push.DeltaTime;

    if (Waiting ? Item.Life >= 0.0 : Item.Life <= 0.0)
    {
        // Respawn at the emitter with a random upward cone velocity.
        const float Angle  = Hash(Item.Seed + Push.Time) * 6.2831853;
        const float Spread = 0.15 + 0.6 * Hash(Item.Seed * 7.13 + Push.Time);
        Item.Position      = Push.Emitter;
        Item.Velocity      = float3(cos(Angle) * Spread, 4.0 + 1.5 * Hash(Item.Seed * 3.7 + Push.Time), sin(Angle) * Spread);
        Item.Life          = 1.6 + Hash(Item.Seed * 1.9);
        Item.Seed          = frac(Item.Seed * 1.618 + 0.137);
    }
    else if (!Waiting)
    {
        Item.Velocity.y -= 9.81 * Push.DeltaTime;
        Item.Position   += Item.Velocity * Push.DeltaTime;
        if (Item.Position.y < 0.0)
        {
            Item.Position.y  = 0.0;
            Item.Velocity.y *= -0.35;
            Item.Velocity.xz *= 0.6;
        }
    }

    Push.Particles[Id.x] = Item;
}

struct VertexOutput
{
    float4 Position : SV_Position;
    float2 Corner   : TEXCOORD0;
    float  Life     : TEXCOORD1;
};

[shader("vertex")]
VertexOutput VertexMain(uint VertexId : SV_VertexID, uint InstanceId : SV_InstanceID)
{
    const float2   Corners[6] = { float2(-1, -1), float2(1, -1), float2(1, 1), float2(-1, -1), float2(1, 1), float2(-1, 1) };
    const Particle Item       = Push.Particles[InstanceId];
    const float2   Corner     = Corners[VertexId];
    const float3   World      = Item.Position + (Push.CameraRight * Corner.x + Push.CameraUp * Corner.y) * Push.Size;

    VertexOutput Output;
    Output.Position = mul(Push.ViewProjection, float4(World, 1.0));
    Output.Corner   = Corner;
    Output.Life     = Item.Life;
    return Output;
}

[shader("fragment")]
float4 FragmentMain(VertexOutput Input) : SV_Target
{
    const float Falloff = saturate(1.0 - dot(Input.Corner, Input.Corner));
    const float3 Color  = lerp(float3(0.15, 0.45, 1.0), float3(0.9, 0.97, 1.0), saturate(Input.Life * 0.6));
    return float4(Color * 1.5, Falloff * saturate(Input.Life * 2.0));
}

// GPU debug lines (luvk::app::DebugDraw::AcquireGpuLines): one velocity line per DebugStride-th live particle.
struct DebugLines
{
    uint VertexCount;
    uint InstanceCount;
    uint FirstVertex;
    uint FirstInstance;
    uint Capacity;
};

struct LineVertex
{
    float3 Position;
    uint   Color;
};

struct DebugPush
{
    Particle*   Particles;
    DebugLines* Lines;
    LineVertex* Vertices;
    uint        Count;
    float       Scale;
};

// Specialization constant 0: one SPIR-V module, the stride is baked in when the pipeline is created.
[vk::constant_id(0)] const uint DebugStride = 16;

[shader("compute")]
[numthreads(64, 1, 1)]
void DebugMain(uint3 Id : SV_DispatchThreadID, uniform DebugPush Debug)
{
    const uint Index = Id.x * DebugStride;
    if (Index >= Debug.Count)
    {
        return;
    }

    const Particle Item = Debug.Particles[Index];
    if (Item.Life <= 0.0)
    {
        return;
    }

    uint First = 0;
    InterlockedAdd(Debug.Lines->VertexCount, 2, First);
    if (First + 2 > Debug.Lines->Capacity)
    {
        return;
    }

    Debug.Vertices[First]     = { Item.Position, 0xFF00FFFFu };
    Debug.Vertices[First + 1] = { Item.Position + Item.Velocity * Debug.Scale, 0xFF0000FFu };
}
)slang";
}
