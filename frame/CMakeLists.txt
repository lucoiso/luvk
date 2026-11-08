# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

# luvk-frame: render features, named phases, typed blackboard slots and the scene frame (scene color/depth, clear,
# composite). Depends only on luvk-core (plus header-only glm): no window, no UI.

INCLUDE("${CMAKE_CURRENT_SOURCE_DIR}/../cmake/FetchGlm.cmake")

FILE(GLOB_RECURSE LUVK_FRAME_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

ADD_LIBRARY(luvk-frame STATIC ${LUVK_FRAME_SOURCES})
ADD_LIBRARY(luvk::frame ALIAS luvk-frame)
TARGET_COMPILE_FEATURES(luvk-frame PUBLIC cxx_std_23)
TARGET_INCLUDE_DIRECTORIES(luvk-frame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
TARGET_LINK_LIBRARIES(luvk-frame PUBLIC luvk::core glm::glm PRIVATE luvk::vulkan)
TARGET_COMPILE_DEFINITIONS(luvk-frame PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS)

LUVK_EMBED_SHADERS(luvk-frame
                   HEADER FrameShaders.hpp
                   NAMESPACE luvk::frame::Shaders
                   SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
                   SHADERS
                   Composite.slang:CompositeVertex:vertex
                   Composite.slang:CompositeFragment:fragment)

LUVK_DEFINE_MODULE_API(luvk-frame LUVK)
LUVK_APPLY_WARNINGS(luvk-frame)
LUVK_SET_OUTPUT_DIRECTORIES(luvk-frame)
