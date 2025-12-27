// Author: Lucas Vilas-Boas
// Year: 2025
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Types/Texture.hpp"

using namespace luvk;

Texture::Texture(std::shared_ptr<Image> Image, std::shared_ptr<Sampler> Sampler)
    : m_Image(std::move(Image)),
      m_Sampler(std::move(Sampler)) {}
