# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

SET(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
SET(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
SET(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)

FETCHCONTENT_DECLARE(vulkan
                     GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
                     GIT_TAG v1.4.367
)
FETCHCONTENT_MAKEAVAILABLE(vulkan)

TARGET_INCLUDE_DIRECTORIES(Vulkan-Headers SYSTEM INTERFACE ${vulkan_SOURCE_DIR}/include)

SET(VOLK_HEADERS_ONLY OFF CACHE BOOL "" FORCE)
SET(VOLK_INSTALL OFF CACHE BOOL "" FORCE)
SET(VOLK_PULL_IN_VULKAN OFF CACHE BOOL "" FORCE)
SET(VOLK_NAMESPACE OFF CACHE BOOL "" FORCE)

FETCHCONTENT_DECLARE(volk
                     GIT_REPOSITORY https://github.com/zeux/volk.git
                     GIT_TAG 1.4.350
)
FETCHCONTENT_MAKEAVAILABLE(volk)

TARGET_SOURCES(volk PRIVATE ${volk_SOURCE_DIR}/volk.c)

# No VK_USE_PLATFORM_* define: luvk never calls a platform-specific Vulkan entry point (surfaces come from
# SDL), and those defines would leak <windows.h> / X11 / XCB headers into every consumer.
TARGET_COMPILE_DEFINITIONS(volk PUBLIC VK_NO_PROTOTYPES=1)

TARGET_INCLUDE_DIRECTORIES(volk SYSTEM INTERFACE ${volk_SOURCE_DIR})
TARGET_LINK_LIBRARIES(volk PUBLIC Vulkan-Headers)
