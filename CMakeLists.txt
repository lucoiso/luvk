# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

CMAKE_MINIMUM_REQUIRED(VERSION 3.28)

PROJECT(luvk VERSION 0.1.0 LANGUAGES C CXX)

OPTION(LUVK_BUILD_FRAME "Build luvk-frame (render features, phases, typed slots and the scene frame)" OFF)
OPTION(LUVK_BUILD_PLATFORM "Build luvk-platform (SDL3 window and input)" OFF)
OPTION(LUVK_BUILD_IMGUI "Build luvk-imgui (Dear ImGui rendered through luvk)" OFF)
OPTION(LUVK_BUILD_APP "Build luvk-app (application skeleton, camera and debug draw)" OFF)
OPTION(LUVK_BUILD_SCENE "Build luvk-scene (meshes, textures and materials on top of the core)" OFF)
OPTION(LUVK_BUILD_TESTS "Build the luvk test suite (not included in this checkout)" OFF)
OPTION(LUVK_BUILD_EXAMPLE "Build luvk-example (full feature showcase)" OFF)
OPTION(LUVK_INCLUDE_SLANG_COMPILER "Build luvk-shaders (runtime Slang to SPIR-V compiler)" OFF)
OPTION(LUVK_IMGUI_DEMO "Compile Dear ImGui's demo window (ImGui::ShowDemoWindow)" OFF)
OPTION(LUVK_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

IF (LUVK_BUILD_EXAMPLE)
    SET(LUVK_BUILD_APP ON)
    SET(LUVK_BUILD_SCENE ON)
    SET(LUVK_INCLUDE_SLANG_COMPILER ON)
    SET(LUVK_IMGUI_DEMO ON)
ENDIF ()

IF (LUVK_BUILD_APP)
    SET(LUVK_BUILD_FRAME ON)
    SET(LUVK_BUILD_PLATFORM ON)
    SET(LUVK_BUILD_IMGUI ON)
ENDIF ()

IF (LUVK_BUILD_IMGUI)
    SET(LUVK_BUILD_PLATFORM ON)
ENDIF ()

# Standalone builds write under build/; inside a parent project its own settings stay in charge. A single-config tree
# (the Ninja presets) gets no per-config output folder and builds its dependencies in its own _deps, so its outputs stay
# in its own tree unless LUVK_OUTPUT_DIR names a folder of its own (the presets use build/binaries/<preset>).
IF (PROJECT_IS_TOP_LEVEL AND CMAKE_CONFIGURATION_TYPES)
    SET(LUVK_DEFAULT_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/build/binaries")
ELSE ()
    SET(LUVK_DEFAULT_OUTPUT_DIR "${CMAKE_BINARY_DIR}")
ENDIF ()

SET(LUVK_OUTPUT_DIR "${LUVK_DEFAULT_OUTPUT_DIR}" CACHE PATH "Root output directory: binaries go to <dir>/bin, libraries to <dir>/lib")

# Dependencies and helper functions are declared before the targets so that the targets can link them.
ADD_SUBDIRECTORY(cmake)

ADD_SUBDIRECTORY(source/core)

IF (LUVK_INCLUDE_SLANG_COMPILER)
    ADD_SUBDIRECTORY(source/shaders)
ENDIF ()

IF (LUVK_BUILD_FRAME)
    ADD_SUBDIRECTORY(source/frame)
ENDIF ()

IF (LUVK_BUILD_SCENE)
    ADD_SUBDIRECTORY(source/scene)
ENDIF ()

IF (LUVK_BUILD_PLATFORM)
    ADD_SUBDIRECTORY(source/platform)
ENDIF ()

IF (LUVK_BUILD_IMGUI)
    ADD_SUBDIRECTORY(source/imgui)
ENDIF ()

IF (LUVK_BUILD_APP)
    ADD_SUBDIRECTORY(source/app)
ENDIF ()

IF (LUVK_BUILD_EXAMPLE)
    ADD_SUBDIRECTORY(examples/example)
ENDIF ()

IF (LUVK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    ENABLE_TESTING()
    ADD_SUBDIRECTORY(tests)
ENDIF ()

