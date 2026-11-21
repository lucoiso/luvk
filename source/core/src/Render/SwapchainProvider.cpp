// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#include "luvk/Render/SwapchainProvider.hpp"

luvk::SwapchainProvider::~SwapchainProvider() = default;

bool luvk::SwapchainProvider::SupportsPresentFences() const noexcept
{
    return false;
}
