// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstdint>
#include <glm/glm.hpp>
#include <luvk/Core/Common.hpp>

namespace luvk
{
    class Image;
}

namespace luvk::scene
{
    class Texture;

    /** Lighting model of a material. */
    enum class Shading : std::uint8_t
    {
        /** Base color (and emission) only. */
        Unlit,

        /** Lambert diffuse + Blinn-Phong specular under the renderer's directional light. */
        Lit,
    };

    /**
     * Surface description used by the MeshRenderer: plain data (no GPU objects), cheap to create and copy. The
     * renderer builds and caches the pipelines a material needs (per target format, wireframe, blending, culling) and
     * declares the material's texture to the frame graph, so a texture written earlier in the same frame (a render
     * target, a ray traced image...) is synchronised automatically.
     */
    struct Material
    {
        /** Lighting model. */
        Shading Model = Shading::Lit;

        /** Linear base color; alpha is used when Transparent is set. */
        glm::vec4 BaseColor{1.F};

        /** Sampled 2D image multiplied with BaseColor (a Texture's image, a render target...); it must outlive the frame. */
        Image* BaseColorTexture = nullptr;

        /** Bindless sampler index for BaseColorTexture; InvalidBindlessIndex uses the renderer's linear repeat sampler. */
        std::uint32_t Sampler = InvalidBindlessIndex;

        /** Linear emitted color added on top of the lighting. */
        glm::vec3 Emissive{0.F};

        /** 0 = mirror-like highlight, 1 = no highlight. */
        float Roughness = 0.5F;

        /** Alpha blended, drawn after the opaque meshes without writing depth. */
        bool Transparent = false;

        /** Draws back faces too (lit with the flipped normal). */
        bool DoubleSided = false;

        /** Sets BaseColorTexture to the image of Source. */
        Material& WithTexture(Texture& Source);
    };
}
