# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

SET(VMA_ENABLE_INSTALL OFF)
SET(VMA_BUILD_DOCUMENTATION OFF)
SET(VMA_BUILD_SAMPLES OFF)

FETCHCONTENT_DECLARE(vma
                     GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
                     GIT_TAG v3.5.0
)
FETCHCONTENT_MAKEAVAILABLE(vma)

TARGET_INCLUDE_DIRECTORIES(VulkanMemoryAllocator SYSTEM INTERFACE ${vma_SOURCE_DIR}/include)
