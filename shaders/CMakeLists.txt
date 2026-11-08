# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

IF (NOT TARGET slang)
    MESSAGE(FATAL_ERROR "luvk-shaders requires Slang (LUVK_INCLUDE_SLANG_COMPILER=ON).")
ENDIF ()

FILE(GLOB_RECURSE LUVK_SHADER_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

ADD_LIBRARY(luvk-shaders STATIC ${LUVK_SHADER_SOURCES})
ADD_LIBRARY(luvk::shaders ALIAS luvk-shaders)
TARGET_COMPILE_FEATURES(luvk-shaders PUBLIC cxx_std_23)
TARGET_INCLUDE_DIRECTORIES(luvk-shaders PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
TARGET_LINK_LIBRARIES(luvk-shaders PUBLIC luvk::core slang)

# The luvk Slang module is compiled into the library: shaders import it without any file on disk.
SET(LUVK_MODULE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/LuvkModuleSource.hpp")
ADD_CUSTOM_COMMAND(OUTPUT "${LUVK_MODULE_HEADER}"
                   COMMAND "${CMAKE_COMMAND}" "-DINPUT=${LUVK_SHADER_MODULE_DIR}/luvk.slang" "-DOUTPUT=${LUVK_MODULE_HEADER}" -DNAME=LuvkModuleSource
                           -P "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EmbedText.cmake"
                   DEPENDS "${LUVK_SHADER_MODULE_DIR}/luvk.slang" "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/EmbedText.cmake"
                   COMMENT "Embedding the luvk Slang module"
                   VERBATIM)
TARGET_SOURCES(luvk-shaders PRIVATE "${LUVK_MODULE_HEADER}")
TARGET_INCLUDE_DIRECTORIES(luvk-shaders PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

LUVK_APPLY_WARNINGS(luvk-shaders)
LUVK_SET_OUTPUT_DIRECTORIES(luvk-shaders)
