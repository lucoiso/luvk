# Author: Lucas Vilas-Boas
# Year: 2025
# Repo: https://github.com/lucoiso/luvk

INCLUDE(FetchContent)
INCLUDE(CMakeParseArguments)

INCLUDE(Utils.cmake)

INCLUDE(FetchVolk.cmake)
INCLUDE(FetchVma.cmake)

IF (LUVK_INCLUDE_SLANG_COMPILER)
    INCLUDE(FetchSlang.cmake)
ENDIF (LUVK_INCLUDE_SLANG_COMPILER)