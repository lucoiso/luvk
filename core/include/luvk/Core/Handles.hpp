// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include "luvk/Core/Common.hpp"

namespace luvk
{
    /**
     * A 32-bit bindless index with its resource kind in the type, so a sampled 2D texture, a cube and a storage image are
     * distinct at compile time. The Slang module declares the same handles (luvk::Texture2D, luvk::StorageImage, ...)
     * with matching accessors, so a push struct written with them has one layout on both sides.
     */
    template <typename Tag>
    struct BindlessHandle
    {
        /** Index into the bindless array of this kind, or InvalidBindlessIndex. */
        std::uint32_t Index = InvalidBindlessIndex;

        /** True when the handle refers to a slot. */
        [[nodiscard]] constexpr bool IsValid() const noexcept { return Index != InvalidBindlessIndex; }

        /** Index equality. */
        [[nodiscard]] friend constexpr bool operator==(const BindlessHandle&, const BindlessHandle&) noexcept = default;
    };

    /** Tag of sampled 2D textures (binding 0). */
    struct Texture2DTag;

    /** Tag of sampled 3D textures (binding 0). */
    struct Texture3DTag;

    /** Tag of sampled cube textures (binding 0). */
    struct TextureCubeTag;

    /** Tag of sampled 2D array textures (binding 0). */
    struct TextureArrayTag;

    /** Tag of storage images (binding 2). */
    struct StorageImageTag;

    /** Tag of samplers (binding 1). */
    struct SamplerTag;

    /** Tag of top-level acceleration structures (binding 3). */
    struct SceneTag;

    /** Sampled 2D texture. */
    using Texture2D = BindlessHandle<Texture2DTag>;

    /** Sampled 3D texture. */
    using Texture3D = BindlessHandle<Texture3DTag>;

    /** Sampled cube texture. */
    using TextureCube = BindlessHandle<TextureCubeTag>;

    /** Sampled 2D array texture. */
    using TextureArray = BindlessHandle<TextureArrayTag>;

    /** Storage image (read-write, any dimension the shader declares). */
    using StorageImage = BindlessHandle<StorageImageTag>;

    /** Sampler. */
    using SamplerHandle = BindlessHandle<SamplerTag>;

    /** Top-level acceleration structure. */
    using SceneHandle = BindlessHandle<SceneTag>;

    static_assert(sizeof(Texture2D) == 4U && sizeof(StorageImage) == 4U && sizeof(SamplerHandle) == 4U);
}
