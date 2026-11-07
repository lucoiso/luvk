# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

INCLUDE(FetchContent)

FETCHCONTENT_DECLARE(imgui
                     GIT_REPOSITORY https://github.com/ocornut/imgui.git
                     GIT_TAG v1.93.0-docking
                     SYSTEM
)
FETCHCONTENT_MAKEAVAILABLE(imgui)

ADD_LIBRARY(luvk-imgui-vendor STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
)
TARGET_INCLUDE_DIRECTORIES(luvk-imgui-vendor SYSTEM PUBLIC ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
TARGET_COMPILE_DEFINITIONS(luvk-imgui-vendor PUBLIC IMGUI_DISABLE_OBSOLETE_FUNCTIONS)

# imgui_demo.cpp compiles to empty stubs unless the demo window is requested.
IF (NOT LUVK_IMGUI_DEMO)
    TARGET_COMPILE_DEFINITIONS(luvk-imgui-vendor PUBLIC IMGUI_DISABLE_DEMO_WINDOWS)
ENDIF ()
TARGET_LINK_LIBRARIES(luvk-imgui-vendor PUBLIC SDL3::SDL3-static)

FILE(GLOB_RECURSE LUVK_IMGUI_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

ADD_LIBRARY(luvk-imgui STATIC ${LUVK_IMGUI_SOURCES})
ADD_LIBRARY(luvk::imgui ALIAS luvk-imgui)
TARGET_COMPILE_FEATURES(luvk-imgui PUBLIC cxx_std_23)
TARGET_INCLUDE_DIRECTORIES(luvk-imgui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
TARGET_LINK_LIBRARIES(luvk-imgui PUBLIC luvk::core luvk::platform luvk-imgui-vendor)

TARGET_COMPILE_DEFINITIONS(luvk-imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
LUVK_APPLY_WARNINGS(luvk-imgui)
LUVK_SET_OUTPUT_DIRECTORIES(luvk-imgui)
