# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

# Slang comes from the installed Vulkan SDK when it ships it (LUVK_SLANG_FROM_VULKAN_SDK, default ON), else it is consumed
# as prebuilt release binaries (see FetchSlangHelper.cmake), so none of the SLANG_* from-source build options apply here.
# Both paths define the same imported target `slang`, the SLANGC cache variable (path of slangc) and
# LUVK_SLANG_RUNTIME_FILES (cache variable: the Slang runtime libraries a consumer copies next to its executable).

OPTION(LUVK_SLANG_FROM_VULKAN_SDK "Use the Slang shipped with the Vulkan SDK (VULKAN_SDK) when available, instead of downloading it" ON)

SET(LUVK_SLANG_RUNTIME_NAMES slang slang-compiler slang-glslang slang-glsl-module slang-rt slang-llvm)

# Lists the Slang runtime libraries present in DIRECTORY (Windows DLLs or shared objects elsewhere) into OUT_VAR.
FUNCTION(LUVK_COLLECT_SLANG_RUNTIME DIRECTORY OUT_VAR)
    SET(FILES)
    FOREACH(NAME IN LISTS LUVK_SLANG_RUNTIME_NAMES)
        IF (WIN32)
            IF (EXISTS "${DIRECTORY}/${NAME}.dll")
                LIST(APPEND FILES "${DIRECTORY}/${NAME}.dll")
            ENDIF ()
        ELSE ()
            FILE(GLOB MATCHES "${DIRECTORY}/lib${NAME}.so*" "${DIRECTORY}/lib${NAME}*.dylib")
            LIST(APPEND FILES ${MATCHES})
        ENDIF ()
    ENDFOREACH()
    SET(${OUT_VAR} "${FILES}" PARENT_SCOPE)
ENDFUNCTION()

SET(LUVK_SLANG_FROM_SDK_FOUND OFF)

IF (LUVK_SLANG_FROM_VULKAN_SDK AND DEFINED ENV{VULKAN_SDK} AND NOT TARGET slang)
    FILE(TO_CMAKE_PATH "$ENV{VULKAN_SDK}" LUVK_VULKAN_SDK_DIR)

    # The Windows SDK uses Include/Lib/Bin, the Linux and macOS SDKs include/lib/bin.
    SET(SDK_INCLUDE_DIR)
    FOREACH(CANDIDATE IN ITEMS "${LUVK_VULKAN_SDK_DIR}/Include" "${LUVK_VULKAN_SDK_DIR}/include")
        IF (NOT SDK_INCLUDE_DIR AND EXISTS "${CANDIDATE}/slang/slang.h")
            SET(SDK_INCLUDE_DIR "${CANDIDATE}/slang")
        ENDIF ()
    ENDFOREACH()

    SET(SDK_IMPLIB)
    SET(SDK_LIBRARY)
    SET(SDK_RUNTIME_DIR)
    SET(SDK_SLANGC)

    IF (WIN32)
        IF (MSVC AND EXISTS "${LUVK_VULKAN_SDK_DIR}/Lib/slang.lib" AND EXISTS "${LUVK_VULKAN_SDK_DIR}/Bin/slang.dll")
            SET(SDK_IMPLIB "${LUVK_VULKAN_SDK_DIR}/Lib/slang.lib")
            SET(SDK_LIBRARY "${LUVK_VULKAN_SDK_DIR}/Bin/slang.dll")
            SET(SDK_RUNTIME_DIR "${LUVK_VULKAN_SDK_DIR}/Bin")
        ENDIF ()
        IF (EXISTS "${LUVK_VULKAN_SDK_DIR}/Bin/slangc.exe")
            SET(SDK_SLANGC "${LUVK_VULKAN_SDK_DIR}/Bin/slangc.exe")
        ENDIF ()
    ELSE ()
        IF (APPLE)
            SET(SDK_LIBRARY_NAMES libslang.dylib libslang-compiler.dylib)
        ELSE ()
            SET(SDK_LIBRARY_NAMES libslang.so libslang-compiler.so)
        ENDIF ()

        FOREACH(LIBRARY_DIR IN ITEMS "${LUVK_VULKAN_SDK_DIR}/lib" "${LUVK_VULKAN_SDK_DIR}/lib64" "${LUVK_VULKAN_SDK_DIR}/Lib")
            FOREACH(LIBRARY_NAME IN LISTS SDK_LIBRARY_NAMES)
                IF (NOT SDK_LIBRARY AND EXISTS "${LIBRARY_DIR}/${LIBRARY_NAME}")
                    SET(SDK_LIBRARY "${LIBRARY_DIR}/${LIBRARY_NAME}")
                    SET(SDK_RUNTIME_DIR "${LIBRARY_DIR}")
                ENDIF ()
            ENDFOREACH()
        ENDFOREACH()

        IF (EXISTS "${LUVK_VULKAN_SDK_DIR}/bin/slangc")
            SET(SDK_SLANGC "${LUVK_VULKAN_SDK_DIR}/bin/slangc")
        ENDIF ()
    ENDIF ()

    IF (SDK_INCLUDE_DIR AND SDK_LIBRARY)
        ADD_LIBRARY(slang SHARED IMPORTED GLOBAL)
        SET_TARGET_PROPERTIES(slang PROPERTIES IMPORTED_LOCATION "${SDK_LIBRARY}")
        IF (SDK_IMPLIB)
            SET_TARGET_PROPERTIES(slang PROPERTIES IMPORTED_IMPLIB "${SDK_IMPLIB}")
        ELSE ()
            SET_TARGET_PROPERTIES(slang PROPERTIES IMPORTED_NO_SONAME TRUE)
        ENDIF ()
        TARGET_INCLUDE_DIRECTORIES(slang INTERFACE "${SDK_INCLUDE_DIR}")

        SET(SLANG_FOUND ON CACHE BOOL "ON if Slang library was found." FORCE)
        SET(SLANG_ROOT "${LUVK_VULKAN_SDK_DIR}" CACHE PATH "Slang root directory." FORCE)
        IF (SDK_SLANGC)
            SET(SLANGC "${SDK_SLANGC}" CACHE FILEPATH "Path to the slangc executable." FORCE)
        ENDIF ()

        LUVK_COLLECT_SLANG_RUNTIME("${SDK_RUNTIME_DIR}" SDK_RUNTIME_FILES)
        SET(LUVK_SLANG_RUNTIME_FILES "${SDK_RUNTIME_FILES}" CACHE INTERNAL "Slang runtime libraries to copy next to executables using luvk-shaders")
        SET(LUVK_SLANG_FROM_SDK_FOUND ON)
        MESSAGE(STATUS "Using Slang from the Vulkan SDK at '${LUVK_VULKAN_SDK_DIR}'")
    ELSE ()
        MESSAGE(STATUS "The Vulkan SDK at '${LUVK_VULKAN_SDK_DIR}' does not ship Slang; downloading the prebuilt release instead")
    ENDIF ()
ENDIF ()

IF (NOT LUVK_SLANG_FROM_SDK_FOUND AND NOT TARGET slang)
    INCLUDE(FetchSlangHelper.cmake)
    FETCH_SLANG(VERSION 2026.20 COMPONENTS slang slang-compiler)

    IF (WIN32)
        LUVK_COLLECT_SLANG_RUNTIME("${SLANG_ROOT}/bin" FETCHED_RUNTIME_FILES)
    ELSE ()
        LUVK_COLLECT_SLANG_RUNTIME("${SLANG_ROOT}/lib" FETCHED_RUNTIME_FILES)
    ENDIF ()
    SET(LUVK_SLANG_RUNTIME_FILES "${FETCHED_RUNTIME_FILES}" CACHE INTERNAL "Slang runtime libraries to copy next to executables using luvk-shaders")
ENDIF ()
