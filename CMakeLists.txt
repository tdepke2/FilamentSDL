cmake_minimum_required(VERSION 3.15...3.30)

include(cmake/CompileProperties.cmake)

# Set project name and version.
project(FilamentSDL
    VERSION 1.0
    DESCRIPTION "Filament with SDL and Dear ImGui"
    LANGUAGES C CXX
)

# Check if this is the main project (not included with add_subdirectory).
if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Set default build type if unspecified.
    # From https://cliutils.gitlab.io/modern-cmake/chapters/features.html
    set(DEFAULT_BUILD_TYPE "Release")
    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Setting build type to '${DEFAULT_BUILD_TYPE}' as none was specified.")
        set(CMAKE_BUILD_TYPE "${DEFAULT_BUILD_TYPE}" CACHE
            STRING "Choose the type of build." FORCE
        )
        # Set the possible values of build type for cmake-gui
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            "Debug" "Release" "MinSizeRel" "RelWithDebInfo"
        )
    elseif(CMAKE_BUILD_TYPE)
        message(STATUS "Current build type is '${CMAKE_BUILD_TYPE}'.")
    elseif(CMAKE_CONFIGURATION_TYPES)
        message(STATUS "Build type is not set, select it during build with '--config' option and one of the following: '${CMAKE_CONFIGURATION_TYPES}'.")
    endif()

    # Place binaries in lib/ or bin/ respectively instead of in the sources directory.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

    # Have CMake create a "compile_commands.json" file for clangd.
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    # Enable support for folders in IDEs.
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)

    # Enable testing by default.
    option(BUILD_TESTING "Build the testing tree." ON)
    message(STATUS "Building tests is '${BUILD_TESTING}'.")
    if (BUILD_TESTING)
        enable_testing()
    endif()

    # SDL3 settings.
    set(SDL_EXAMPLES OFF)

    set(SDL_AUDIO OFF)
    set(SDL_CAMERA OFF)
    set(SDL_DIALOG OFF)
    set(SDL_GPU OFF)
    set(SDL_HAPTIC OFF)
    set(SDL_HIDAPI OFF)
    set(SDL_JOYSTICK OFF)
    set(SDL_POWER OFF)
    set(SDL_RENDER OFF)
    set(SDL_SENSOR OFF)
    set(SDL_VIDEO ON)

    message("")
endif()



include(cmake/FetchArchive.cmake)
#set(FETCHCONTENT_QUIET OFF)

# Import Simple DirectMedia Layer for the window system and other utilities.
fetch_archive_declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL
    GIT_TAG release-3.2.26
)
list(APPEND FETCH_PENDING "SDL3")

# Import Dear ImGui for GUI library.
fetch_archive_declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui
    GIT_TAG v1.92.5
)
list(APPEND FETCH_PENDING "imgui")

# Populate all of the pending content.
foreach(CONTENT_NAME IN LISTS FETCH_PENDING)
    message(STATUS "Fetching sources for ${CONTENT_NAME}...")
    fetch_archive_make_available(${CONTENT_NAME})
    message("")
endforeach()
unset(FETCH_PENDING)

include(cmake/ImGui.cmake)

#add_library(filament STATIC IMPORTED)
#set_target_properties(filament PROPERTIES
#    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/filament-v1.72.0-windows/lib/x86_64/md/filament.lib")
#target_include_directories(filament INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/filament-v1.72.0-windows/include")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
find_package(Filament "1.72.0" REQUIRED)

# Run the next CMakeLists file (builds the executable).
add_subdirectory(src)
