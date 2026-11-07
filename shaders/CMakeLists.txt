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

LUVK_APPLY_WARNINGS(luvk-shaders)
LUVK_SET_OUTPUT_DIRECTORIES(luvk-shaders)
