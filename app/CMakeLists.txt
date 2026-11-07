# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

FILE(GLOB_RECURSE LUVK_APP_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")

# The mesh feature only exists when the optional scene module is built.
IF (NOT TARGET luvk-scene)
    LIST(FILTER LUVK_APP_SOURCES EXCLUDE REGEX "MeshFeature\.(cpp|hpp)$")
ENDIF ()

ADD_LIBRARY(luvk-app STATIC ${LUVK_APP_SOURCES})
ADD_LIBRARY(luvk::app ALIAS luvk-app)
TARGET_COMPILE_FEATURES(luvk-app PUBLIC cxx_std_23)
TARGET_INCLUDE_DIRECTORIES(luvk-app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
TARGET_LINK_LIBRARIES(luvk-app PUBLIC luvk::core luvk::platform luvk::imgui glm::glm)
IF (TARGET luvk-scene)
    TARGET_LINK_LIBRARIES(luvk-app PUBLIC luvk::scene)
ENDIF ()

TARGET_COMPILE_DEFINITIONS(luvk-app PRIVATE _CRT_SECURE_NO_WARNINGS)
TARGET_COMPILE_DEFINITIONS(luvk-app PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE GLM_FORCE_RADIANS SDL_MAIN_HANDLED
                           $<$<CONFIG:Debug>:LUVK_APP_VALIDATION_DEFAULT=1>)

LUVK_APPLY_WARNINGS(luvk-app)
LUVK_SET_OUTPUT_DIRECTORIES(luvk-app)
