# Author: Lucas Vilas-Boas
# Year: 2025
# Repo: https://github.com/lucoiso/luvk

SET(VMA_ENABLE_INSTALL OFF)
SET(VMA_BUILD_DOCUMENTATION OFF)
SET(VMA_BUILD_SAMPLES OFF)

FETCHCONTENT_DECLARE(vma
                     GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
                     GIT_TAG v3.3.0
)
FETCHCONTENT_MAKEAVAILABLE(vma)

TARGET_INCLUDE_DIRECTORIES(VulkanMemoryAllocator INTERFACE ${vma_SOURCE_DIR}/include)

TARGET_LINK_LIBRARIES(${LIBRARY_NAME} PUBLIC VulkanMemoryAllocator)