# Author: Lucas Vilas-Boas
# Year: 2025
# Repo: https://github.com/lucoiso/luvk

INCLUDE(FetchContent)
INCLUDE(CMakeParseArguments)

FUNCTION(DETECT_ARCH TARGET_OR_HOST OUT_VAR)
    IF (TARGET_OR_HOST STREQUAL "TARGET")
        SET(SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR})
        IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
            SET(IS_64BIT ON)
        ELSEIF (CMAKE_SIZEOF_VOID_P EQUAL 4)
            SET(IS_64BIT OFF)
        ENDIF()
    ELSEIF(TARGET_OR_HOST STREQUAL "HOST")
        SET(SYSTEM_PROCESSOR ${CMAKE_HOST_SYSTEM_PROCESSOR})
        CMAKE_HOST_SYSTEM_INFORMATION(RESULT IS_64BIT QUERY IS_64BIT)
    ELSE()
        MESSAGE(FATAL_ERROR "First argument of DETECT_ARCH() must be TARGET or HOST.")
    ENDIF()

    IF (SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x86")
        IF (IS_64BIT)
            SET(ARCH "x86_64")
        ELSE()
            SET(ARCH "i686")
        ENDIF()
    ELSEIF (SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|armv8|arm)$")
        SET(ARCH "aarch64")
    ELSEIF(SYSTEM_PROCESSOR MATCHES "^(armv7|armv6|armhf)$")
        SET(ARCH "arm")
    ELSE()
        MESSAGE(WARNING "Unknown architecture: ${SYSTEM_PROCESSOR}")
        SET(ARCH "unknown")
    ENDIF()
    SET(${OUT_VAR} ${ARCH} PARENT_SCOPE)
ENDFUNCTION(DETECT_ARCH)

FUNCTION(FETCH_SLANG)
    CMAKE_PARSE_ARGUMENTS(
            SLANG_ARGS
            ""
            "VERSION;MIRROR"
            "COMPONENTS"
            ${ARGN}
    )

    IF (NOT SLANG_ARGS_VERSION)
        MESSAGE(FATAL_ERROR "FETCH_SLANG requires the VERSION argument.")
    ENDIF()

    IF (NOT SLANG_ARGS_MIRROR)
        SET(SLANG_MIRROR_URL "https://github.com/shader-slang/slang")
    ELSE()
        SET(SLANG_MIRROR_URL ${SLANG_ARGS_MIRROR})
    ENDIF()

    SET(SLANG_LIB_REQUESTED FALSE)
    SET(SLANGC_EXE_REQUESTED FALSE)
    FOREACH(COMP IN LISTS SLANG_ARGS_COMPONENTS)
        IF (COMP STREQUAL "slang")
            SET(SLANG_LIB_REQUESTED TRUE)
        ENDIF()
        IF (COMP STREQUAL "slang-compiler")
            SET(SLANGC_EXE_REQUESTED TRUE)
        ENDIF()
    ENDFOREACH()

    SET(URL_OS)
    SET(SLANG_LIB_FOUND ${SLANG_LIB_REQUESTED})
    IF (SLANG_LIB_REQUESTED)
        IF (CMAKE_SYSTEM_NAME STREQUAL "Windows")
            SET(URL_OS "windows")
        ELSEIF (CMAKE_SYSTEM_NAME STREQUAL "Linux")
            SET(URL_OS "linux")
        ELSEIF (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            SET(URL_OS "macos")
        ELSE()
            MESSAGE(STATUS "Target system '${CMAKE_SYSTEM_NAME}' not supported. Disabling Slang library.")
            SET(SLANG_LIB_FOUND OFF)
        ENDIF()
    ENDIF()

    IF (SLANG_LIB_FOUND)
        DETECT_ARCH(TARGET ARCH)
        SET(URL_NAME "slang-${SLANG_ARGS_VERSION}-${URL_OS}-${ARCH}")
        STRING(TOLOWER "${URL_NAME}" FC_NAME)
        SET(URL "${SLANG_MIRROR_URL}/releases/download/v${SLANG_ARGS_VERSION}/${URL_NAME}.zip")

        FETCHCONTENT_DECLARE(${FC_NAME} URL ${URL})
        MESSAGE(STATUS "Using Slang target binaries from '${URL}'")
        FETCHCONTENT_MAKEAVAILABLE(${FC_NAME})
        SET(SLANG_ROOT "${${FC_NAME}_SOURCE_DIR}")
    ENDIF (SLANG_LIB_FOUND)

    SET(HOST_URL_OS)
    SET(SLANG_EXE_FOUND ${SLANGC_EXE_REQUESTED})
    IF (SLANGC_EXE_REQUESTED)
        IF (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            SET(HOST_URL_OS "windows")
        ELSEIF (CMAKE_HOST_SYSTEM_NAME STREQUAL "Linux")
            SET(HOST_URL_OS "linux")
        ELSEIF (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
            SET(HOST_URL_OS "macos")
        ELSE()
            MESSAGE(STATUS "Host system '${CMAKE_HOST_SYSTEM_NAME}' not supported. Disabling Slang executable.")
            SET(SLANG_EXE_FOUND OFF)
        ENDIF()
    ENDIF()

    IF (SLANG_EXE_FOUND)
        DETECT_ARCH(HOST HOST_ARCH)
        SET(URL_NAME "slang-${SLANG_ARGS_VERSION}-${HOST_URL_OS}-${HOST_ARCH}")
        STRING(TOLOWER "${URL_NAME}" HOST_FC_NAME)
        SET(HOST_URL "${SLANG_MIRROR_URL}/releases/download/v${SLANG_ARGS_VERSION}/${URL_NAME}.zip")

        IF (SLANG_LIB_FOUND AND HOST_URL STREQUAL URL)
            SET(SLANG_HOST_ROOT ${SLANG_ROOT})
        ELSE()
            FETCHCONTENT_DECLARE(${HOST_FC_NAME} URL ${HOST_URL})
            MESSAGE(STATUS "Using Slang host binaries from '${HOST_URL}'")
            FETCHCONTENT_MAKEAVAILABLE(${HOST_FC_NAME})
            SET(SLANG_HOST_ROOT "${${HOST_FC_NAME}_SOURCE_DIR}")
        ENDIF()
    ENDIF (SLANG_EXE_FOUND)

    IF (SLANG_LIB_FOUND)
        ADD_LIBRARY(slang SHARED IMPORTED GLOBAL)

        IF (CMAKE_SYSTEM_NAME STREQUAL "Windows")
            IF (MSVC)
                SET_TARGET_PROPERTIES(
                        slang
                        PROPERTIES
                        IMPORTED_IMPLIB "${SLANG_ROOT}/lib/slang.lib"
                        IMPORTED_LOCATION "${SLANG_ROOT}/bin/slang.dll"
                )
            ELSE()
                MESSAGE(FATAL_ERROR "Slang does not provide precompiled binaries for MSYS/MinGW.")
            ENDIF()
        ELSEIF (CMAKE_SYSTEM_NAME STREQUAL "Linux")
            SET_TARGET_PROPERTIES(
                    slang
                    PROPERTIES
                    IMPORTED_LOCATION "${SLANG_ROOT}/lib/libslang.so"
                    IMPORTED_NO_SONAME TRUE
            )
        ELSEIF (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            SET_TARGET_PROPERTIES(
                    slang
                    PROPERTIES
                    IMPORTED_LOCATION "${SLANG_ROOT}/lib/libslang.dylib"
            )
        ENDIF()

        TARGET_INCLUDE_DIRECTORIES(slang INTERFACE
                                   "${SLANG_ROOT}/include"
        )
    ENDIF (SLANG_LIB_FOUND)

    SET(SLANG_FOUND "${SLANG_LIB_FOUND}" PARENT_SCOPE CACHE BOOL "ON if Slang library was found.")

    IF (SLANGC_EXE_FOUND)
        IF (CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
            SET(EXE_EXT ".exe")
        ELSE()
            SET(EXE_EXT)
        ENDIF()
        SET(SLANGC "${SLANG_HOST_ROOT}/bin/slangc${EXE_EXT}" PARENT_SCOPE CACHE PATH "Path to the slangc executable.")
    ENDIF (SLANGC_EXE_FOUND)

ENDFUNCTION(FETCH_SLANG)