# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

FILE(GLOB_RECURSE LUVK_CORE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
FILE(GLOB_RECURSE LUVK_CORE_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/luvk/*.hpp")

ADD_LIBRARY(luvk-core STATIC ${LUVK_CORE_SOURCES} ${LUVK_CORE_HEADERS})
ADD_LIBRARY(luvk::core ALIAS luvk-core)
ADD_LIBRARY(luvk::luvk ALIAS luvk-core)
SET_TARGET_PROPERTIES(luvk-core PROPERTIES OUTPUT_NAME luvk)

TARGET_COMPILE_FEATURES(luvk-core PUBLIC cxx_std_23)
SET_TARGET_PROPERTIES(luvk-core PROPERTIES
                      LINKER_LANGUAGE CXX
                      CXX_EXTENSIONS OFF
                      EXPORT_NAME core)

TARGET_INCLUDE_DIRECTORIES(luvk-core
                           PUBLIC
                           $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                           $<INSTALL_INTERFACE:include>
                           PRIVATE
                           ${CMAKE_CURRENT_SOURCE_DIR}/src
)

TARGET_LINK_LIBRARIES(luvk-core PUBLIC volk VulkanMemoryAllocator)

IF (WIN32)
    TARGET_COMPILE_DEFINITIONS(luvk-core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
ELSEIF (APPLE)
    TARGET_COMPILE_OPTIONS(luvk-core PUBLIC -fexperimental-library)
ENDIF ()

LUVK_DEFINE_MODULE_API(luvk-core LUVK)
LUVK_APPLY_WARNINGS(luvk-core)
LUVK_SET_OUTPUT_DIRECTORIES(luvk-core)

INSTALL(TARGETS luvk-core
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
)
INSTALL(DIRECTORY include/ DESTINATION include)

# Package-config export is intentionally not installed yet: the public core headers expose volk and
# VulkanMemoryAllocator, which are fetched dependencies and must be packaged first. External projects
# can consume luvk with add_subdirectory/FetchContent in the meantime.
