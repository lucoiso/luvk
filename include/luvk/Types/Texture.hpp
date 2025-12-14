// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>
#include "luvk/Module.hpp"

namespace luvk
{
    class Image;
    class Sampler;

    class LUVKMODULE_API Texture
    {
        std::shared_ptr<Image>   m_Image{};
        std::shared_ptr<Sampler> m_Sampler{};

    public:
        Texture() = delete;
        explicit Texture(std::shared_ptr<Image> Image, std::shared_ptr<Sampler> Sampler);

        [[nodiscard]] constexpr const std::shared_ptr<Image>& GetImage() const noexcept
        {
            return m_Image;
        }

        [[nodiscard]] constexpr const std::shared_ptr<Sampler>& GetSampler() const noexcept
        {
            return m_Sampler;
        }
    };
} // namespace luvk
