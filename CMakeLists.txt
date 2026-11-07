# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

CMAKE_MINIMUM_REQUIRED(VERSION 3.28)

PROJECT(luvk VERSION 0.1.0 LANGUAGES C CXX)

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
    SET(LUVK_BUILD_PLATFORM ON)
    SET(LUVK_BUILD_IMGUI ON)
ENDIF ()

IF (LUVK_BUILD_IMGUI)
    SET(LUVK_BUILD_PLATFORM ON)
ENDIF ()

SET(LUVK_OUTPUT_DIR "${CMAKE_BINARY_DIR}" CACHE PATH "Root output directory: binaries go to <dir>/bin, libraries to <dir>/lib")

# Dependencies and helper functions are declared before the targets so that the targets can link them.
ADD_SUBDIRECTORY(cmake)

ADD_SUBDIRECTORY(core)

IF (LUVK_INCLUDE_SLANG_COMPILER)
    ADD_SUBDIRECTORY(shaders)
ENDIF ()

IF (LUVK_BUILD_SCENE)
    ADD_SUBDIRECTORY(scene)
ENDIF ()

IF (LUVK_BUILD_PLATFORM)
    ADD_SUBDIRECTORY(platform)
ENDIF ()

IF (LUVK_BUILD_IMGUI)
    ADD_SUBDIRECTORY(imgui)
ENDIF ()

IF (LUVK_BUILD_APP)
    ADD_SUBDIRECTORY(app)
ENDIF ()

IF (LUVK_BUILD_EXAMPLE)
    ADD_SUBDIRECTORY(example)
ENDIF ()

IF (LUVK_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    ENABLE_TESTING()
    ADD_SUBDIRECTORY(tests)
ENDIF ()

# Regenerates the precompiled SPIR-V embedded in luvk-app / luvk-imgui from app/shaders and imgui/shaders.
# Run it after editing those GLSL files: cmake --build <build> --target luvk-embedded-shaders
FIND_PROGRAM(LUVK_GLSLC glslc HINTS "$ENV{VULKAN_SDK}/Bin" "$ENV{VULKAN_SDK}/bin")
FIND_PROGRAM(LUVK_POWERSHELL NAMES pwsh powershell)
IF (LUVK_GLSLC AND LUVK_POWERSHELL)
    ADD_CUSTOM_TARGET(luvk-embedded-shaders
                      COMMAND "${LUVK_POWERSHELL}" -NoProfile -ExecutionPolicy Bypass
                              -File "${CMAKE_CURRENT_SOURCE_DIR}/tools/generate-embedded-shaders.ps1" -Glslc "${LUVK_GLSLC}"
                      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                      COMMENT "Regenerating the embedded SPIR-V of luvk-app and luvk-imgui"
                      VERBATIM)
ENDIF ()
