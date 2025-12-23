// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <memory>

namespace luvk
{
    class Image;
    class Sampler;

    class LUVK_API Texture
    {
    protected:
        std::shared_ptr<Image>   m_Image{};
        std::shared_ptr<Sampler> m_Sampler{};

    public:
        Texture() = delete;
        explicit Texture(std::shared_ptr<Image> Image, std::shared_ptr<Sampler> Sampler);

        [[nodiscard]] std::shared_ptr<Image> GetImage() const noexcept
        {
            return m_Image;
        }

        [[nodiscard]] std::shared_ptr<Sampler> GetSampler() const noexcept
        {
            return m_Sampler;
        }
    };
} // namespace luvk
