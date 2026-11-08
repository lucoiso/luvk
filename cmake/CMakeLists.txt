# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

INCLUDE(FetchContent)
INCLUDE(CMakeParseArguments)

INCLUDE(Utils.cmake)

INCLUDE(FetchVolk.cmake)
INCLUDE(FetchVma.cmake)
INCLUDE(FetchSpirvReflect.cmake)

# slangc compiles luvk's own shaders (app, imgui, scene) at build time; the runtime compiler (luvk-shaders) uses the same Slang.
IF (LUVK_INCLUDE_SLANG_COMPILER OR LUVK_BUILD_FRAME OR LUVK_BUILD_IMGUI OR LUVK_BUILD_APP OR LUVK_BUILD_SCENE)
    INCLUDE(FetchSlang.cmake)
    INCLUDE(LuvkShaders.cmake)
ENDIF ()
