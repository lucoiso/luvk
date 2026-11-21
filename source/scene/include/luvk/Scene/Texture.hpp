// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <luvk/Resources/Image.hpp>

namespace luvk
{
    class Context;
}

namespace luvk::scene
{
    /** Creation options of a Texture. */
    struct TextureDesc
    {
        /** Interprets the RGBA8 pixels as sRGB (colors) or linear (normals, masks, data). */
        bool Srgb = true;

        /** Builds the whole mip chain (box filter, on the CPU) so the texture stays stable when minified. */
        bool GenerateMips = true;

        /** Optional debug name. */
        std::string_view Name;
    };

    /**
     * A sampled image made from pixels: uploaded once, with its mip chain, and registered in the bindless heap.
     * Materials reference it through SampledIndex(); Application::UiTexture shows it in Dear ImGui.
     */
    class Texture
    {
    public:
        /** Empty texture. */
        Texture() = default;

        /** 2D texture from Width x Height RGBA8 pixels (4 bytes per pixel, rows tightly packed). */
        [[nodiscard]] static Texture FromPixels(Context& Ctx, std::span<const std::byte> Pixels, std::uint32_t Width, std::uint32_t Height,
                                                const TextureDesc& Desc = {});

        /** Cube texture from six Size x Size RGBA8 faces stored back to back (+X, -X, +Y, -Y, +Z, -Z). */
        [[nodiscard]] static Texture CubeFromFaces(Context& Ctx, std::span<const std::byte> Faces, std::uint32_t Size, const TextureDesc& Desc = {});

        /** True when the texture holds an image. */
        [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(m_Image); }

        /** Bindless index to sample the texture with (Textures[] / TextureCubes[] at binding 0). */
        [[nodiscard]] std::uint32_t SampledIndex() const noexcept { return m_Image.SampledIndex(); }

        /** Underlying image. */
        [[nodiscard]] Image& GetImage() noexcept { return m_Image; }

        /** Underlying image. */
        [[nodiscard]] const Image& GetImage() const noexcept { return m_Image; }

    private:
        /** Creates the image and uploads every face and mip level of RGBA8 pixels. */
        static Texture Create(Context& Ctx, std::span<const std::byte> Pixels, std::uint32_t Width, std::uint32_t Height, std::uint32_t Layers,
                              bool Cube, const TextureDesc& Desc);

        /** Image with the pixels and their mip chain. */
        Image m_Image;
    };
}
