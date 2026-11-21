// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <string_view>

namespace Shaders::ShaderPlayground
{
    /** Slang source for the playground fullscreen triangle vertex shader. */
    inline constexpr std::string_view VertexSource = R"slang(
struct VSOutput
{
    float4 Position : SV_Position;
    float2 fragUV   : TEXCOORD0;
};

[shader("vertex")]
VSOutput main(uint VertexId : SV_VertexID)
{
    const float2 UV = float2((VertexId << 1) & 2, VertexId & 2);

    VSOutput Output;
    Output.Position = float4(UV * 2.0 - 1.0, 0.0, 1.0);
    Output.fragUV   = UV;
    return Output;
}
)slang";

    /** Default Slang fragment source shown in the editor on startup. */
    inline constexpr std::string_view DefaultFragmentSource = R"slang(
struct VSOutput {
    float2 fragUV;
};

struct PC {
    float time;
};

[[vk::push_constant]] PC pc;

#define PIXEL_SIZE_FAC 700.0f
#define BLACK (0.6f * float4(79.0f/255.0f, 99.0f/255.0f, 103.0f/255.0f, 1.0f/0.6f))

static const float VortSpeed = 1.0f;
static const float MidFlash = 0.0f;
static const float VortOffset = 0.0f;

static const float4 Colour1 = float4(0.9960784f, 0.3725490f, 0.3333333f, 1.0f);
static const float4 Colour2 = float4(0.0f, 0.6156863f, 1.0f, 1.0f);

float4 Easing(float4 t, float power)
{
    return float4(
       pow(t.x, power),
       pow(t.y, power),
       pow(t.z, power),
       pow(t.w, power)
    );
}

float4 Effect(float2 uv, float scale)
{
    uv = floor(uv * (PIXEL_SIZE_FAC * 0.5f)) / (PIXEL_SIZE_FAC * 0.5f);
    uv /= scale;

    float uvLen = length(uv);

    float speed = pc.time * VortSpeed;

    float angle =
       atan2(uv.y, uv.x)
       + (2.2f + 0.4f * min(6.0f, speed)) * uvLen
       - 1.0f
       - speed * 0.05f
       - min(6.0f, speed) * speed * 0.02f
       + VortOffset;

    float2 sv = float2(
       uvLen * cos(angle),
       uvLen * sin(angle)
    );

    sv *= 30.0f;
    speed = pc.time * 6.0f * VortSpeed + VortOffset + 5.0f;

    float2 uv2 = float2(sv.x + sv.y);

    for (int i = 0; i < 5; ++i)
    {
       uv2 += sin(max(sv.x, sv.y)) + sv;

       sv += 0.5f * float2(
          cos(5.1123314f + 0.353f * uv2.y + speed * 0.131121f),
          sin(uv2.x - 0.113f * speed)
       );

       sv -= 1.0f * cos(sv.x + sv.y)
          - 1.0f * sin(sv.x * 0.711f - sv.y);
    }

    float smoke =
       min(2.0f,
       max(-2.0f,
          1.5f + length(sv) * 0.12f
          - 0.17f * min(10.0f, pc.time * 1.2f)
       ));

    if (smoke < 0.2f)
    {
       smoke = (smoke - 0.2f) * 0.6f + 0.2f;
    }

    float c1p = max(0.0f, 1.0f - 2.0f * abs(1.0f - smoke));
    float c2p = max(0.0f, 1.0f - 2.0f * smoke);
    float cb = 1.0f - min(1.0f, c1p + c2p);

    float4 col =
       Colour1 * c1p +
       Colour2 * c2p +
       float4(cb * BLACK.rgb, cb * Colour1.a);

    float modFlash =
       max(MidFlash * 0.8f, max(c1p, c2p) * 5.0f - 4.4f)
       + MidFlash * max(c1p, c2p);

    return Easing(
       col * (1.0f - modFlash) + modFlash * float4(1.0f),
       1.5f
    );
}

[shader("fragment")]
float4 main(VSOutput input) : SV_Target
{
    float2 uv = input.fragUV * 2.0f - 1.0f;
    uv.x *= 1.0f;
    float4 col = Effect(uv, 2.0f);

    return float4(sqrt(clamp(col.rgb, 0.0f, 1.0f)), col.a);
}
)slang";
}
