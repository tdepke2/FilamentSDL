# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html
# https://cmake.org/cmake/help/latest/manual/cmake-developer.7.html
# https://discourse.cmake.org/t/combining-two-third-party-libraries-into-a-single-imported-target/5695

message(STATUS "TGD: start FindFilament")

# FIXME: add each of the sub-libraries in a loop (or with a macro)? The list of these should be provided from the caller through a variable.
# FIXME: add support and test on linux.

# Currently only Windows is supported in this find script.

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
        if(NOT DOWNLOAD_STATUS_CODE} EQUAL 0)
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




#message(FATAL_ERROR "TGD: die")



set(Filament_INCLUDE_DIR "${Filament_ROOT_DIR}/include")
set(Filament_LIBRARY
    "${Filament_ROOT_DIR}/lib/x86_64/mdd/filament.lib"
)

add_library(Filament::Filament STATIC IMPORTED)
set_target_properties(Filament::Filament PROPERTIES
    IMPORTED_LOCATION "${Filament_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Filament_INCLUDE_DIR}"
)

add_library(Filament::FilamentUtils STATIC IMPORTED)
set_target_properties(Filament::FilamentUtils PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/utils.lib"
)

add_library(Filament::FilamentBackend STATIC IMPORTED)
set_target_properties(Filament::FilamentBackend PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/backend.lib"
)

add_library(Filament::FilamentBluegl STATIC IMPORTED)
set_target_properties(Filament::FilamentBluegl PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/bluegl.lib"
)

add_library(Filament::FilamentBluevk STATIC IMPORTED)
set_target_properties(Filament::FilamentBluevk PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/bluevk.lib"
)

add_library(Filament::FilamentFilabridge STATIC IMPORTED)
set_target_properties(Filament::FilamentFilabridge PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/filabridge.lib"
)

add_library(Filament::FilamentFilaflat STATIC IMPORTED)
set_target_properties(Filament::FilamentFilaflat PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/filaflat.lib"
)

add_library(Filament::FilamentGeometry STATIC IMPORTED)
set_target_properties(Filament::FilamentGeometry PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/geometry.lib"
)

add_library(Filament::FilamentSmolv STATIC IMPORTED)
set_target_properties(Filament::FilamentSmolv PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/smol-v.lib"
)

add_library(Filament::FilamentIbl STATIC IMPORTED)
set_target_properties(Filament::FilamentIbl PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/ibl.lib"
)

add_library(Filament::FilamentAbseil STATIC IMPORTED)
set_target_properties(Filament::FilamentAbseil PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/abseil.lib"
)

add_library(Filament::FilamentZstd STATIC IMPORTED)
set_target_properties(Filament::FilamentZstd PROPERTIES
    IMPORTED_LOCATION "${Filament_ROOT_DIR}/lib/x86_64/mdd/zstd.lib"
)

find_package(OpenGL)

add_library(Filament::FilamentAll INTERFACE IMPORTED)
target_link_libraries(Filament::FilamentAll INTERFACE
    Filament::Filament
    Filament::FilamentUtils
    Filament::FilamentBackend
    Filament::FilamentBluegl
    Filament::FilamentBluevk
    Filament::FilamentFilabridge
    Filament::FilamentFilaflat
    Filament::FilamentGeometry
    Filament::FilamentSmolv
    Filament::FilamentIbl
    Filament::FilamentAbseil
    Filament::FilamentZstd
    OpenGL::GL
)
