// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

// The luvk bindless interface. Every luvk pipeline uses one descriptor set (set 0) and 128 bytes of push constants;
// resources are indices into the arrays below and buffers are device addresses. `import luvk;` exposes:
//   - typed handles matching the C++ ones (luvk::Texture2D, Texture3D, TextureCube, TextureArray, StorageImage,
//     SamplerHandle): four bytes each, so a push struct written with them has one layout on both sides;
//   - sampling, loading and storing helpers for each handle kind;
//   - luvk::Span<T>: a device address plus an element count with bounds-checked Load/Store (the C++ twin is
//     luvk::GpuSpan<T>). Robustness does not cover device addresses, so every indexed access through a pointer goes
//     through a span.
// Binding 0 aliases sampled views of every dimension, binding 2 storage views; the arrays are partially bound.

module luvk;

[[vk::binding(0, 0)]] public Texture2D                 LuvkTextures[];
[[vk::binding(0, 0)]] public Texture2D<uint4>          LuvkUintTextures[];
[[vk::binding(0, 0)]] public Texture3D                 LuvkTextures3D[];
[[vk::binding(0, 0)]] public Texture3D<uint4>          LuvkUintTextures3D[];
[[vk::binding(0, 0)]] public TextureCube               LuvkTextureCubes[];
[[vk::binding(0, 0)]] public Texture2DArray            LuvkTextureArrays[];
[[vk::binding(1, 0)]] public SamplerState              LuvkSamplers[];
[[vk::binding(2, 0)]] public RWTexture2D<float4>       LuvkImages[];
[[vk::binding(2, 0)]] public RWTexture2D<uint4>        LuvkUintImages[];
[[vk::binding(2, 0)]] public RWTexture2D<uint>         LuvkUintImages1[];
[[vk::binding(2, 0)]] public RWTexture3D<float4>       LuvkImages3D[];
[[vk::binding(2, 0)]] public RWTexture3D<uint4>        LuvkUintImages3D[];
[[vk::binding(2, 0)]] public RWTexture2DArray<float4>  LuvkImageArrays[];

// Reflection attributes: read by luvk::ShaderCompiler (ShaderField::Attributes) to drive editors and bindings, e.g.
//     [Range(0.0, 1.0)] [Tooltip("Strength of the effect")] float Strength;
//     [Slot("SceneColor")] luvk::Texture2D Source;
[__AttributeUsage(_AttributeTargets.Var)]
public struct RangeAttribute
{
    float Min;
    float Max;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct StepAttribute
{
    float Value;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct LabelAttribute
{
    string Text;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct TooltipAttribute
{
    string Text;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct UnitAttribute
{
    string Text;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct SlotAttribute
{
    string Name;
};

[__AttributeUsage(_AttributeTargets.Var)]
public struct AdvancedAttribute
{
};

namespace luvk
{
    /** Sampler (binding 1). */
    public struct SamplerHandle
    {
        public uint Index;
    }

    /** Sampled 2D texture (binding 0). */
    public struct Texture2D
    {
        public uint Index;

        public float4 Sample(SamplerHandle Sampler, float2 UV)
        {
            return LuvkTextures[NonUniformResourceIndex(Index)].Sample(LuvkSamplers[NonUniformResourceIndex(Sampler.Index)], UV);
        }

        public float4 SampleLevel(SamplerHandle Sampler, float2 UV, float Level)
        {
            return LuvkTextures[NonUniformResourceIndex(Index)].SampleLevel(LuvkSamplers[NonUniformResourceIndex(Sampler.Index)], UV, Level);
        }

        public float4 Load(int2 Texel, int Level = 0)
        {
            return LuvkTextures[NonUniformResourceIndex(Index)].Load(int3(Texel, Level));
        }

        public uint4 LoadUint(int2 Texel, int Level = 0)
        {
            return LuvkUintTextures[NonUniformResourceIndex(Index)].Load(int3(Texel, Level));
        }

        public float4 Gather(SamplerHandle Sampler, float2 UV)
        {
            return LuvkTextures[NonUniformResourceIndex(Index)].Gather(LuvkSamplers[NonUniformResourceIndex(Sampler.Index)], UV);
        }

        public uint2 Size(int Level = 0)
        {
            uint Width = 0;
            uint Height = 0;
            uint Levels = 0;
            LuvkTextures[NonUniformResourceIndex(Index)].GetDimensions(Level, Width, Height, Levels);
            return uint2(Width, Height);
        }
    }

    /** Sampled 3D texture (binding 0). */
    public struct Texture3D
    {
        public uint Index;

        public float4 SampleLevel(SamplerHandle Sampler, float3 UVW, float Level = 0.0)
        {
            return LuvkTextures3D[NonUniformResourceIndex(Index)].SampleLevel(LuvkSamplers[NonUniformResourceIndex(Sampler.Index)], UVW, Level);
        }

        public float4 Load(int3 Texel, int Level = 0)
        {
            return LuvkTextures3D[NonUniformResourceIndex(Index)].Load(int4(Texel, Level));
        }

        public uint4 LoadUint(int3 Texel, int Level = 0)
        {
            return LuvkUintTextures3D[NonUniformResourceIndex(Index)].Load(int4(Texel, Level));
        }
    }

    /** Sampled cube texture (binding 0). */
    public struct TextureCube
    {
        public uint Index;

        public float4 Sample(SamplerHandle Sampler, float3 Direction)
        {
            return LuvkTextureCubes[NonUniformResourceIndex(Index)].Sample(LuvkSamplers[NonUniformResourceIndex(Sampler.Index)], Direction);
        }

        public float4 SampleLevel(SamplerHandle Sampler, float3 Direction, float Level)
        {
            return LuvkTextureCubes[NonUniformResourceIndex(Index)].SampleLevel(LuvkSamplers[NonUniformResourceIndex(Sampler.Index)], Direction, Level);
        }
    }

    /** Sampled 2D array texture (binding 0). */
    public struct TextureArray
    {
        public uint Index;

        public float4 Sample(SamplerHandle Sampler, float2 UV, float Layer)
        {
            return LuvkTextureArrays[NonUniformResourceIndex(Index)].Sample(LuvkSamplers[NonUniformResourceIndex(Sampler.Index)], float3(UV, Layer));
        }

        public float4 SampleLevel(SamplerHandle Sampler, float2 UV, float Layer, float Level)
        {
            return LuvkTextureArrays[NonUniformResourceIndex(Index)].SampleLevel(LuvkSamplers[NonUniformResourceIndex(Sampler.Index)], float3(UV, Layer), Level);
        }
    }

    /** Storage image (binding 2): the view's dimension and texel type are chosen by the accessor. */
    public struct StorageImage
    {
        public uint Index;

        public float4 Load(int2 Texel)
        {
            return LuvkImages[NonUniformResourceIndex(Index)][Texel];
        }

        public void Store(int2 Texel, float4 Value)
        {
            LuvkImages[NonUniformResourceIndex(Index)][Texel] = Value;
        }

        public uint4 LoadUint(int2 Texel)
        {
            return LuvkUintImages[NonUniformResourceIndex(Index)][Texel];
        }

        public void StoreUint(int2 Texel, uint4 Value)
        {
            LuvkUintImages[NonUniformResourceIndex(Index)][Texel] = Value;
        }

        public uint AtomicMax(int2 Texel, uint Value)
        {
            uint Previous = 0;
            InterlockedMax(LuvkUintImages1[NonUniformResourceIndex(Index)][Texel], Value, Previous);
            return Previous;
        }

        public float4 Load3D(int3 Texel)
        {
            return LuvkImages3D[NonUniformResourceIndex(Index)][Texel];
        }

        public void Store3D(int3 Texel, float4 Value)
        {
            LuvkImages3D[NonUniformResourceIndex(Index)][Texel] = Value;
        }

        public uint4 LoadUint3D(int3 Texel)
        {
            return LuvkUintImages3D[NonUniformResourceIndex(Index)][Texel];
        }

        public void StoreUint3D(int3 Texel, uint4 Value)
        {
            LuvkUintImages3D[NonUniformResourceIndex(Index)][Texel] = Value;
        }

        public float4 LoadLayer(int2 Texel, int Layer)
        {
            return LuvkImageArrays[NonUniformResourceIndex(Index)][int3(Texel, Layer)];
        }

        public void StoreLayer(int2 Texel, int Layer, float4 Value)
        {
            LuvkImageArrays[NonUniformResourceIndex(Index)][int3(Texel, Layer)] = Value;
        }

        public uint2 Size()
        {
            uint Width = 0;
            uint Height = 0;
            LuvkImages[NonUniformResourceIndex(Index)].GetDimensions(Width, Height);
            return uint2(Width, Height);
        }
    }

    /**
     * A device address plus an element count (16 bytes, the twin of luvk::GpuSpan<T>). Load returns a default value and
     * Store does nothing for an index outside [0, Count): a bad index never reaches memory.
     */
    public struct Span<T>
    {
        public T* Data;
        public uint Count;
        public uint Reserved;

        public bool Contains(uint Index)
        {
            return Index < Count;
        }

        public T Load(uint Index)
        {
            if (Index < Count)
            {
                return Data[Index];
            }
            T Default;
            return Default;
        }

        public void Store(uint Index, T Value)
        {
            if (Index < Count)
            {
                Data[Index] = Value;
            }
        }

        /** Pointer to an element for atomics on its members; Index is clamped into the span (Count must be > 0). */
        public T* At(uint Index)
        {
            return Data + min(Index, max(Count, 1u) - 1u);
        }
    }

    /** Sampled 2D texture by raw index (code written before the typed handles). */
    public float4 Sample(uint Texture, uint Sampler, float2 UV)
    {
        return LuvkTextures[NonUniformResourceIndex(Texture)].Sample(LuvkSamplers[NonUniformResourceIndex(Sampler)], UV);
    }

    /** Sampled cube texture by raw index. */
    public float4 SampleCube(uint Texture, uint Sampler, float3 Direction)
    {
        return LuvkTextureCubes[NonUniformResourceIndex(Texture)].Sample(LuvkSamplers[NonUniformResourceIndex(Sampler)], Direction);
    }
}
