// Author: Lucas Vilas-Boas
// Year: 2026
// Repo : https://github.com/lucoiso/luvk

#pragma once

#include <mutex>
#include <slang-com-ptr.h>
#include <slang.h>

namespace luvk
{
    /** Process-wide Slang session state (created lazily on first compilation). */
    struct SlangState
    {
        /** Mutex serializing session access across compilations. */
        std::mutex Mutex;

        /** Global Slang session shared by every compilation. */
        Slang::ComPtr<slang::IGlobalSession> GlobalSession;

        /** Result of the global session creation (SLANG_OK on success). */
        SlangResult Result = SLANG_FAIL;

        /** Constructor. Creates the global Slang session. */
        SlangState()
        {
            Result = slang::createGlobalSession(GlobalSession.writeRef());
        }
    };
}
