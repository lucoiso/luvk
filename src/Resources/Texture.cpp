/*
* Author: Lucas Vilas-Boas
 * Year: 2025
 * Repo: https://github.com/lucoiso/luvk
 */

#include "luvk/Resources/Texture.hpp"

using namespace luvk;

Texture::Texture(const std::shared_ptr<Image>& Image, const std::shared_ptr<Sampler>& Sampler, const std::string_view Name)
    : m_Image(Image),
      m_Sampler(Sampler),
      m_Name(Name) {}
