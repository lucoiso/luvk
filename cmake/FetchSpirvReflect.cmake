# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

# SPIRV-Reflect is one C file; it is compiled here instead of through its own CMake project (which builds tools and tests).
FETCHCONTENT_DECLARE(spirv_reflect
                     GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
                     GIT_TAG vulkan-sdk-1.4.363.0
                     GIT_SHALLOW TRUE
                     SOURCE_SUBDIR luvk-no-cmake
)
FETCHCONTENT_MAKEAVAILABLE(spirv_reflect)

ADD_LIBRARY(luvk-spirv-reflect STATIC "${spirv_reflect_SOURCE_DIR}/spirv_reflect.c")
TARGET_INCLUDE_DIRECTORIES(luvk-spirv-reflect SYSTEM PUBLIC "${spirv_reflect_SOURCE_DIR}")
SET_TARGET_PROPERTIES(luvk-spirv-reflect PROPERTIES FOLDER "luvk/third-party")
LUVK_SET_OUTPUT_DIRECTORIES(luvk-spirv-reflect)
