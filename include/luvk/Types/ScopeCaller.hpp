// Author: Lucas Vilas-Boas
// Year: 2025
// Repo : https://github.com/lucoiso/luvk

#pragma once

namespace luvk
{
    template <auto Functor>
    struct ScopeCaller
    {
        ~ScopeCaller()
        {
            Functor();
        }
    };
} // namespace luvk
