// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include "luvk/Modules/Module.hpp"

#include <memory>

namespace luvk
{
    class Image;
    class Sampler;

    /** Simple texture handle combining image and sampler */
    class LUVKMODULE_API Texture
    {
        std::shared_ptr<Image> m_Image{};
        std::shared_ptr<Sampler> m_Sampler{};

    public:
        constexpr Texture() = default;

        explicit Texture(std::shared_ptr<Image> img, std::shared_ptr<Sampler> samp) noexcept
            : m_Image(std::move(img)),
              m_Sampler(std::move(samp)) {}

        [[nodiscard]] constexpr std::shared_ptr<Image> const& GetImage() const noexcept
        {
            return m_Image;
        }

        [[nodiscard]] constexpr std::shared_ptr<Sampler> const& GetSampler() const noexcept
        {
            return m_Sampler;
        }
    };
} // namespace luvk
