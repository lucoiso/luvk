// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Scene/Texture.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
#include <vector>

namespace
{
    /** Next mip level of RGBA8 pixels: 2x2 box filter, clamped at odd edges. */
    std::vector<std::byte> Downsample(const std::span<const std::byte> Source, const std::uint32_t Width, const std::uint32_t Height)
    {
        const std::uint32_t    TargetWidth  = std::max(1U, Width / 2U);
        const std::uint32_t    TargetHeight = std::max(1U, Height / 2U);
        std::vector<std::byte> Result(static_cast<std::size_t>(TargetWidth) * TargetHeight * 4U);

        for (std::uint32_t Y = 0U; Y < TargetHeight; ++Y)
        {
            for (std::uint32_t X = 0U; X < TargetWidth; ++X)
            {
                const std::uint32_t X0 = std::min(X * 2U, Width - 1U);
                const std::uint32_t X1 = std::min(X * 2U + 1U, Width - 1U);
                const std::uint32_t Y0 = std::min(Y * 2U, Height - 1U);
                const std::uint32_t Y1 = std::min(Y * 2U + 1U, Height - 1U);

                for (std::uint32_t Channel = 0U; Channel < 4U; ++Channel)
                {
                    const auto Texel = [&](const std::uint32_t TexelX, const std::uint32_t TexelY)
                    {
                        return static_cast<std::uint32_t>(Source[(static_cast<std::size_t>(TexelY) * Width + TexelX) * 4U + Channel]);
                    };
                    const std::uint32_t Sum = Texel(X0, Y0) + Texel(X1, Y0) + Texel(X0, Y1) + Texel(X1, Y1);
                    Result[(static_cast<std::size_t>(Y) * TargetWidth + X) * 4U + Channel] = static_cast<std::byte>((Sum + 2U) / 4U);
                }
            }
        }
        return Result;
    }
}

luvk::scene::Texture luvk::scene::Texture::FromPixels(Context&                         Ctx,
                                                      const std::span<const std::byte> Pixels,
                                                      const std::uint32_t              Width,
                                                      const std::uint32_t              Height,
                                                      const TextureDesc&               Desc)
{
    return Create(Ctx, Pixels, Width, Height, 1U, false, Desc);
}

luvk::scene::Texture luvk::scene::Texture::CubeFromFaces(Context& Ctx, const std::span<const std::byte> Faces, const std::uint32_t Size, const TextureDesc& Desc)
{
    return Create(Ctx, Faces, Size, Size, 6U, true, Desc);
}

luvk::scene::Texture luvk::scene::Texture::Create(Context&                         Ctx,
                                                  const std::span<const std::byte> Pixels,
                                                  const std::uint32_t              Width,
                                                  const std::uint32_t              Height,
                                                  const std::uint32_t              Layers,
                                                  const bool                       Cube,
                                                  const TextureDesc&               Desc)
{
    const std::size_t LayerBytes = static_cast<std::size_t>(Width) * Height * 4U;
    if (Width == 0U || Height == 0U || std::size(Pixels) != LayerBytes * Layers)
    {
        throw std::invalid_argument("[luvk-scene] Texture pixels must hold Width x Height RGBA8 texels per layer.");
    }

    const std::uint32_t Mips = Desc.GenerateMips ? static_cast<std::uint32_t>(std::bit_width(std::max(Width, Height))) : 1U;

    Texture Result;
    Result.m_Image = Image(Ctx,
                           ImageDesc{.Extent      = {Width, Height, 1U},
                                     .Format      = Desc.Srgb ? Format::R8G8B8A8Srgb : Format::R8G8B8A8Unorm,
                                     .Usage       = ImageUsage::Sampled,
                                     .MipLevels   = Mips,
                                     .ArrayLayers = Layers,
                                     .Cube        = Cube,
                                     .Name        = Desc.Name});

    for (std::uint32_t Layer = 0U; Layer < Layers; ++Layer)
    {
        std::vector<std::byte> Level(std::begin(Pixels) + static_cast<std::ptrdiff_t>(LayerBytes * Layer),
                                     std::begin(Pixels) + static_cast<std::ptrdiff_t>(LayerBytes * (Layer + 1U)));
        std::uint32_t          LevelWidth  = Width;
        std::uint32_t          LevelHeight = Height;

        for (std::uint32_t Mip = 0U; Mip < Mips; ++Mip)
        {
            Result.m_Image.Upload(Level, Mip, Layer);
            if (Mip + 1U < Mips)
            {
                Level       = Downsample(Level, LevelWidth, LevelHeight);
                LevelWidth  = std::max(1U, LevelWidth / 2U);
                LevelHeight = std::max(1U, LevelHeight / 2U);
            }
        }
    }

    return Result;
}
