# Author: Lucas Vilas-Boas
# Year: 2026
# Repo: https://github.com/lucoiso/luvk

# Build-time Slang -> SPIR-V for code that ships its shaders inside the binary (luvk's own app, imgui and scene layers).
#
# LUVK_EMBED_SHADERS(<target>
#                    HEADER    <file name of the generated header, e.g. AppShaders.hpp>
#                    NAMESPACE <C++ namespace of the accessors, e.g. luvk::app::Shaders>
#                    SOURCE_DIR <directory of the .slang files>
#                    SHADERS   <File.slang:Entry:stage>...)
#
# Every entry is compiled by slangc (the luvk module of core/shaders on the include path) into <build>/luvk-shaders/,
# then a script turns the SPIR-V into a generated source with one accessor per entry point:
#     std::span<const std::uint32_t> <NAMESPACE>::<Entry>() noexcept;
# The header goes to a private include directory of the target.

SET(LUVK_SHADER_MODULE_DIR "${CMAKE_CURRENT_LIST_DIR}/../core/shaders" CACHE INTERNAL "Directory of luvk.slang")
SET(LUVK_EMBED_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/EmbedSpirv.cmake" CACHE INTERNAL "SPIR-V embedding script")

IF (NOT SLANGC)
    MESSAGE(FATAL_ERROR "slangc was not found: luvk compiles its shaders at build time (install the Vulkan SDK or set SLANGC).")
ENDIF ()

FUNCTION(LUVK_EMBED_SHADERS TARGET)
    CMAKE_PARSE_ARGUMENTS(ARG "" "HEADER;NAMESPACE;SOURCE_DIR" "SHADERS" ${ARGN})

    SET(OUTPUT_DIR "${CMAKE_BINARY_DIR}/luvk-shaders/${TARGET}")
    SET(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/${TARGET}")
    FILE(MAKE_DIRECTORY "${OUTPUT_DIR}" "${GENERATED_DIR}")
    FILE(GLOB SLANG_SOURCES CONFIGURE_DEPENDS "${ARG_SOURCE_DIR}/*.slang")

    SET(SPIRV_FILES)
    SET(MANIFEST)
    FOREACH (SHADER IN LISTS ARG_SHADERS)
        STRING(REPLACE ":" ";" PARTS "${SHADER}")
        LIST(GET PARTS 0 FILE_NAME)
        LIST(GET PARTS 1 ENTRY)
        LIST(GET PARTS 2 STAGE)

        SET(OUTPUT "${OUTPUT_DIR}/${ENTRY}.spv")
        ADD_CUSTOM_COMMAND(OUTPUT "${OUTPUT}"
                           COMMAND "${SLANGC}" "${ARG_SOURCE_DIR}/${FILE_NAME}" -target spirv -entry "${ENTRY}" -stage "${STAGE}"
                                   -O2 -matrix-layout-column-major -I "${ARG_SOURCE_DIR}" -I "${LUVK_SHADER_MODULE_DIR}"
                                   -warnings-disable 39001 -o "${OUTPUT}"
                           DEPENDS ${SLANG_SOURCES} "${LUVK_SHADER_MODULE_DIR}/luvk.slang"
                           COMMENT "slangc ${FILE_NAME}:${ENTRY} (${STAGE})"
                           VERBATIM)
        LIST(APPEND SPIRV_FILES "${OUTPUT}")
        LIST(APPEND MANIFEST "${ENTRY}")
    ENDFOREACH ()

    STRING(REPLACE ";" "|" MANIFEST_STRING "${MANIFEST}")
    SET(SOURCE "${GENERATED_DIR}/${TARGET}-shaders.cpp")
    SET(HEADER "${GENERATED_DIR}/${ARG_HEADER}")
    ADD_CUSTOM_COMMAND(OUTPUT "${SOURCE}" "${HEADER}"
                       COMMAND "${CMAKE_COMMAND}" "-DSOURCE=${SOURCE}" "-DHEADER=${HEADER}" "-DHEADER_NAME=${ARG_HEADER}"
                               "-DNAMESPACE=${ARG_NAMESPACE}" "-DSPIRV_DIR=${OUTPUT_DIR}" "-DMANIFEST=${MANIFEST_STRING}"
                               -P "${LUVK_EMBED_SCRIPT}"
                       DEPENDS ${SPIRV_FILES} "${LUVK_EMBED_SCRIPT}"
                       COMMENT "Embedding the SPIR-V of ${TARGET}"
                       VERBATIM)

    TARGET_SOURCES(${TARGET} PRIVATE "${SOURCE}" "${HEADER}")
    TARGET_INCLUDE_DIRECTORIES(${TARGET} PRIVATE "${GENERATED_DIR}")
    SOURCE_GROUP("Generated" FILES "${SOURCE}" "${HEADER}")
    TARGET_SOURCES(${TARGET} PRIVATE ${SLANG_SOURCES})
    SET_SOURCE_FILES_PROPERTIES(${SLANG_SOURCES} PROPERTIES HEADER_FILE_ONLY ON)
    SOURCE_GROUP("Shaders" FILES ${SLANG_SOURCES})
ENDFUNCTION ()
