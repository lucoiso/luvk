// Author: Lucas Vilas-Boas
// Year: 2026
// Repo: https://github.com/lucoiso/luvk

#include "luvk/Scene/Material.hpp"
#include "luvk/Scene/Texture.hpp"

luvk::scene::Material& luvk::scene::Material::WithTexture(Texture& Source)
{
    BaseColorTexture = &Source.GetImage();
    return *this;
}
