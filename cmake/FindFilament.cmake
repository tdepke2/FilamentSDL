
# Finds (or fetches) the given Filament release and creates library targets for
# Filament components. Some helper functions are provided as well.

# Currently only Windows is supported in this find script.
# FIXME: add support and test on linux.

# Filament releases are on the github page here: https://github.com/google/filament

# CMake help for find_package():
# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html
# https://discourse.cmake.org/t/combining-two-third-party-libraries-into-a-single-imported-target/5695



# Search for Filament binaries in "extern/" (or download/extract them). Sets
# Filament_ROOT_DIR in the parent scope.
function(_filament_populate_content)
    if(NOT DEFINED Filament_FIND_VERSION)
        message(FATAL_ERROR "FindFilament.cmake: no version specified.")
    endif()

    set(Filament_FILENAME "filament-v${Filament_FIND_VERSION}-windows")
    set(Filament_FILE_EXT ".tgz")
    set(Filament_ROOT_DIR "${PROJECT_SOURCE_DIR}/extern/${Filament_FILENAME}")
    set(Filament_ROOT_DIR
        "${Filament_ROOT_DIR}"
        PARENT_SCOPE
    )

    if(EXISTS "${Filament_ROOT_DIR}")
        return()
    endif()

    file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/extern")
    if(NOT EXISTS "${Filament_ROOT_DIR}${Filament_FILE_EXT}")
        message(STATUS "Downloading ${Filament_FILENAME}...")
        file(DOWNLOAD
            "https://github.com/google/filament/releases/download/v${Filament_FIND_VERSION}/${Filament_FILENAME}${Filament_FILE_EXT}"
            "${Filament_ROOT_DIR}${Filament_FILE_EXT}"
            STATUS DOWNLOAD_STATUS
        )

        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_STATUS_CODE)
        list(GET DOWNLOAD_STATUS 1 DOWNLOAD_STATUS_ERR)
        if(NOT ${DOWNLOAD_STATUS_CODE} EQUAL 0)
            message(FATAL_ERROR "FindFilament.cmake: failed to download file. ${DOWNLOAD_STATUS_ERR}")
        endif()
    endif()

    message(STATUS "Unpacking: ${Filament_FILENAME}${Filament_FILE_EXT}")
    file(MAKE_DIRECTORY "${Filament_ROOT_DIR}")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${Filament_ROOT_DIR}${Filament_FILE_EXT}"
        WORKING_DIRECTORY "${Filament_ROOT_DIR}"
    )
endfunction()

_filament_populate_content()

set(FILAMENT_LIBS
    Abseil
    Backend
    Basis_transcoder
    Bluegl
    Bluevk
    Camutils
    Dracodec
    Filabridge
    Filaflat
    Filamat
    Filament-iblprefilter
    Filameshio
    Geometry
    Gltfio
    Gltfio_core
    Ibl
    Ktxreader
    Shaders
    Smol-v
    Uberzlib
    Utils
    Zstd
)
set(FILAMENT_LIBS_PATH "${Filament_ROOT_DIR}/lib/x86_64/mdd")
set(FILAMENT_LIBS_PREFIX "")
set(FILAMENT_LIBS_SUFFIX ".lib")

set(Filament_INCLUDE_DIR "${Filament_ROOT_DIR}/include")
set(Filament_LIBRARY
    "${FILAMENT_LIBS_PATH}/${FILAMENT_LIBS_PREFIX}filament${FILAMENT_LIBS_SUFFIX}"
)

add_library(Filament::Filament STATIC IMPORTED)
set_target_properties(Filament::Filament PROPERTIES
    IMPORTED_LOCATION "${Filament_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Filament_INCLUDE_DIR}"
)

# Add a library for an existing filament lib. This can be called by external
# code for libs that are not in the FILAMENT_LIBS list.
function(filament_register_lib LIB_NAME)
    string(TOLOWER "${LIB_NAME}" LIB_NAME_LOWER)
    add_library(Filament::${LIB_NAME} STATIC IMPORTED)
    set_target_properties(Filament::${LIB_NAME} PROPERTIES
        IMPORTED_LOCATION "${FILAMENT_LIBS_PATH}/${FILAMENT_LIBS_PREFIX}${LIB_NAME_LOWER}${FILAMENT_LIBS_SUFFIX}"
    )
endfunction()

foreach(LIB_NAME ${FILAMENT_LIBS})
    filament_register_lib("${LIB_NAME}")
endforeach()

find_package(OpenGL REQUIRED)
target_link_libraries(Filament::Bluegl INTERFACE OpenGL::GL)

if(WIN32)
    # Needed for shlwapi.h
    target_link_libraries(Filament::Utils INTERFACE Shlwapi)
endif()

# add_library(Filament::All INTERFACE IMPORTED)
# target_link_libraries(Filament::All INTERFACE
#     Filament::Filament
#     Filament::Utils
#     Filament::Backend
#     Filament::Bluegl
#     Filament::Bluevk
#     Filament::Filabridge
#     Filament::Filaflat
#     Filament::Filameshio
#     Filament::Geometry
#     Filament::Smolv
#     Filament::Ibl
#     Filament::Abseil
#     Filament::Zstd
#     OpenGL::GL
# )

# Find the path to each of the tools we may use during the build.
foreach(TOOL_NAME cmgen filamesh matc mipgen resgen)
    string(TOUPPER "${TOOL_NAME}" TOOL_NAME_UPPER)
    find_program(FILAMENT_TOOLS_${TOOL_NAME_UPPER} "${TOOL_NAME}" HINTS "${Filament_ROOT_DIR}/bin" REQUIRED)
endforeach()

# Compile some files using the given program. This is designed to be used with
# Filament tools like matc, filamesh, etc. Creates a directory named OUTPUT_DIR
# and appends to OUTPUT_VAR with the list of the generated output files.
function(filament_compile_objects PROG_PATH)
    cmake_parse_arguments(PARSE_ARGV 1 ARG
        "" "OUTPUT_DIR;OUTPUT_VAR;COMMENT_PREFIX;FILE_EXT;PROG_OUT_FLAG" "PROG_FLAGS;SOURCES"
    )
    if(NOT DEFINED ARG_OUTPUT_DIR)
        message(FATAL_ERROR "filament_compile_objects() no output directory specified.")
    endif()
    if(NOT DEFINED ARG_OUTPUT_VAR)
        message(FATAL_ERROR "filament_compile_objects() no output variable specified.")
    endif()
    if(NOT DEFINED ${ARG_OUTPUT_VAR})
        message(FATAL_ERROR "filament_compile_objects() output variable ${ARG_OUTPUT_VAR} must be set.")
    endif()
    if(NOT DEFINED ARG_COMMENT_PREFIX)
        set(ARG_COMMENT_PREFIX "Compiling")
    endif()
    if(NOT DEFINED ARG_SOURCES)
        message(FATAL_ERROR "filament_compile_objects() no source files specified.")
    endif()

    file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")

    foreach(SOURCE ${ARG_SOURCES})
        get_filename_component(LOCALNAME "${SOURCE}" NAME_WE)
        get_filename_component(FULLNAME "${SOURCE}" ABSOLUTE)
        set(OUTPUT_PATH "${ARG_OUTPUT_DIR}/${LOCALNAME}${ARG_FILE_EXT}")

        # Assume the program arguments are <input> <output> unless output flag is specified.
        if(NOT DEFINED ARG_PROG_OUT_FLAG)
            set(FULLNAME_AND_OUTPUT "${FULLNAME}" "${OUTPUT_PATH}")
        else()
            set(FULLNAME_AND_OUTPUT ${ARG_PROG_OUT_FLAG} "${OUTPUT_PATH}" "${FULLNAME}")
        endif()

        add_custom_command(
            OUTPUT "${OUTPUT_PATH}"
            COMMAND ${PROG_PATH} ${ARG_PROG_FLAGS} ${FULLNAME_AND_OUTPUT}
            MAIN_DEPENDENCY "${SOURCE}"
            DEPENDS ${PROG_PATH}
            COMMENT "${ARG_COMMENT_PREFIX} ${SOURCE} to ${OUTPUT_PATH}"
        )
        list(APPEND ${ARG_OUTPUT_VAR} "${OUTPUT_PATH}")
    endforeach()

    set(${ARG_OUTPUT_VAR} ${${ARG_OUTPUT_VAR}} PARENT_SCOPE)
endfunction()

# Compile material definitions with matc.
macro(filament_compile_materials)
    filament_compile_objects("${FILAMENT_TOOLS_MATC}"
        COMMENT_PREFIX "Compiling material"
        FILE_EXT ".filamat"
        PROG_OUT_FLAG -o
        ${ARGV}
    )
endmacro()

# Convert meshes to an optimized format with filamesh.
macro(filament_compile_meshes)
    filament_compile_objects("${FILAMENT_TOOLS_FILAMESH}"
        COMMENT_PREFIX "Compiling mesh"
        FILE_EXT ".filamesh"
        ${ARGV}
    )
endmacro()

# Generate mipmaps for textures, optionally converting to a compressed texture format.
macro(filament_compile_textures)
    filament_compile_objects("${FILAMENT_TOOLS_MIPGEN}"
        COMMENT_PREFIX "Compiling texture"
        ${ARGV}
    )
endmacro()

# Combine resource blobs (such as compiled materials) into a resource file that
# can be embedded into the application. Creates a directory named OUTPUT_DIR and
# sets the variable OUTPUT_VAR with the path to the resource file.
function(filament_resource_gen)
    cmake_parse_arguments(PARSE_ARGV 0 ARG
        "" "OUTPUT_DIR;OUTPUT_VAR;ARCHIVE_NAME" "RESOURCES"
    )
    if(NOT DEFINED ARG_OUTPUT_DIR)
        message(FATAL_ERROR "filament_resource_gen() no output directory specified.")
    endif()
    if(NOT DEFINED ARG_OUTPUT_VAR)
        message(FATAL_ERROR "filament_resource_gen() no output variable specified.")
    endif()
    if(NOT DEFINED ARG_ARCHIVE_NAME)
        set(ARG_ARCHIVE_NAME "resources")
    endif()
    if(NOT DEFINED ARG_RESOURCES)
        message(FATAL_ERROR "filament_resource_gen() no resources specified.")
    endif()

    file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")

    set(RESGEN_OUTPUTS
        "${ARG_OUTPUT_DIR}/${ARG_ARCHIVE_NAME}.bin"
        "${ARG_OUTPUT_DIR}/${ARG_ARCHIVE_NAME}.c"
        "${ARG_OUTPUT_DIR}/${ARG_ARCHIVE_NAME}.h"
        "${ARG_OUTPUT_DIR}/${ARG_ARCHIVE_NAME}.S"
        "${ARG_OUTPUT_DIR}/${ARG_ARCHIVE_NAME}.apple.S"
    )
    set(RESGEN_FLAGS -qcx ${ARG_OUTPUT_DIR} -p ${ARG_ARCHIVE_NAME})

    add_custom_command(
        OUTPUT ${RESGEN_OUTPUTS}
        COMMAND ${FILAMENT_TOOLS_RESGEN} ${RESGEN_FLAGS} ${ARG_RESOURCES}
        DEPENDS ${FILAMENT_TOOLS_RESGEN} ${ARG_RESOURCES}
        COMMENT "Aggregating resources"
    )

    set(${ARG_OUTPUT_VAR} "${ARG_OUTPUT_DIR}/${ARG_ARCHIVE_NAME}.c" PARENT_SCOPE)
endfunction()
