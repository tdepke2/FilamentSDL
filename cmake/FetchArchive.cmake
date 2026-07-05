include(FetchContent)

# Extends the functionality of the FetchContent module to obtain a package (or
# other content) from a local archive instead of from an online repository. When
# a new version of a package is requested, it will be downloaded and archived in
# "extern/". The archive can be checked into this repository and then future
# builds will use this local copy. This makes the build reliable as online
# repositories can go away, but it's useful to know where a certain package came
# from and makes it easy to upgrade the version.
#
# Similar to FetchContent, the content declaration is separated from population.
# This allows all of the packages to be defined up front and identify shared
# dependencies without populating the same package multiple times. Also, like
# the `<lowercaseName>_SOURCE_DIR` and related variables that
# FetchContent_MakeAvailable() defines, fetch_archive_declare() will set
# `<lowercaseName>_ARCHIVE_FILENAME` in the scope of the caller.
#
# The FETCHARCHIVE_FORCE_DOWNLOAD option can be used to override the archive
# behavior, and make these functions behave like the normal FetchContent
# functions.

option(FETCHARCHIVE_FORCE_DOWNLOAD "Prevent usage of local archive, content will always be downloaded" OFF)



function(_pre_fetch_archive_declare CONTENT_NAME)
    cmake_parse_arguments(PARSE_ARGV 1 ARG
        "" "GIT_TAG;URL_MD5" ""
    )

    if(DEFINED ARG_GIT_TAG)
        string(SUBSTRING "${ARG_GIT_TAG}" 0 16 CONTENT_VERSION)
    elseif(DEFINED ARG_URL_MD5)
        string(SUBSTRING "${ARG_URL_MD5}" 0 16 CONTENT_VERSION)
    else()
        message(FATAL_ERROR "fetch_archive_declare() for ${CONTENT_NAME} cannot determine version.")
    endif()

    string(TOLOWER "${CONTENT_NAME}" CONTENT_NAME_LOWER)
    set(${CONTENT_NAME_LOWER}_ARCHIVE_FILENAME
        "${PROJECT_SOURCE_DIR}/extern/${CONTENT_NAME}-${CONTENT_VERSION}.zip"
        PARENT_SCOPE
    )
endfunction()

macro(fetch_archive_declare)
    _pre_fetch_archive_declare(${ARGV})
    FetchContent_Declare(${ARGV})
endmacro()



function(_pre_fetch_archive_make_available CONTENT_NAME)
    string(TOLOWER "${CONTENT_NAME}" CONTENT_NAME_LOWER)
    string(TOUPPER "${CONTENT_NAME}" CONTENT_NAME_UPPER)

    if(NOT DEFINED ${CONTENT_NAME_LOWER}_ARCHIVE_FILENAME)
        message(FATAL_ERROR "fetch_archive_make_available() for ${CONTENT_NAME} failed: ${CONTENT_NAME} must be declared with fetch_archive_declare() first.")
    endif()

    if(FETCHARCHIVE_FORCE_DOWNLOAD)
        return()
    endif()

    if(FETCHCONTENT_QUIET)
        set(ARCHIVE_OPTIONS "f")
    else()
        set(ARCHIVE_OPTIONS "vf")
    endif()
    set(SOURCE_DIR_EXPECTED "${FETCHCONTENT_BASE_DIR}/${CONTENT_NAME_LOWER}-src")

    if(EXISTS "${${CONTENT_NAME_LOWER}_ARCHIVE_FILENAME}")
        if(NOT EXISTS "${SOURCE_DIR_EXPECTED}")
            if(NOT FETCHCONTENT_QUIET)
                message(STATUS "Archive found, unpacking: ${${CONTENT_NAME_LOWER}_ARCHIVE_FILENAME}")
            endif()
            file(MAKE_DIRECTORY "${SOURCE_DIR_EXPECTED}")
            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar "x${ARCHIVE_OPTIONS}" "${${CONTENT_NAME_LOWER}_ARCHIVE_FILENAME}" --format=zip
                WORKING_DIRECTORY "${SOURCE_DIR_EXPECTED}"
            )
        elseif(NOT FETCHCONTENT_QUIET)
            message(STATUS "Archive found and already unpacked.")
        endif()

        set(FETCHCONTENT_SOURCE_DIR_${CONTENT_NAME_UPPER}
            "${SOURCE_DIR_EXPECTED}" CACHE PATH
            "When not empty, overrides where to find pre-populated content for ${CONTENT_NAME}"
        )
    else()
        # When FetchContent is run with FETCHCONTENT_SOURCE_DIR_<uppercaseName> set, it seems to ignore checking if the new package version is different from the existing one.
        if(NOT "${FETCHCONTENT_SOURCE_DIR_${CONTENT_NAME_UPPER}}" STREQUAL "")
            message(FATAL_ERROR "fetch_archive_make_available() for ${CONTENT_NAME} archive not found but source dir already set, did you change a package version? This should only be done with a clean build.")
        endif()

        if(NOT FETCHCONTENT_QUIET)
            message(STATUS "Archive not found, fetching...")
        endif()
    endif()
endfunction()

function(_post_fetch_archive_make_available CONTENT_NAME)
    string(TOLOWER "${CONTENT_NAME}" CONTENT_NAME_LOWER)

    if(FETCHARCHIVE_FORCE_DOWNLOAD OR EXISTS "${${CONTENT_NAME_LOWER}_ARCHIVE_FILENAME}")
        return()
    endif()

    if(FETCHCONTENT_QUIET)
        set(ARCHIVE_OPTIONS "f")
    else()
        set(ARCHIVE_OPTIONS "vf")
    endif()
    set(SOURCE_DIR_EXPECTED "${FETCHCONTENT_BASE_DIR}/${CONTENT_NAME_LOWER}-src")

    # Usually FetchContent_MakeAvailable() sets <lowercaseName>_SOURCE_DIR to point to the source dir, but for some packages this seems to fail.
    # We can get this variable another way using FetchContent_GetProperties().
    FetchContent_GetProperties(${CONTENT_NAME}
        SOURCE_DIR SOURCE_DIR_ACTUAL
    )

    # Could use cmake_path() for this comparison (introduced in CMake 3.20).
    if(NOT "${SOURCE_DIR_ACTUAL}" STREQUAL "${SOURCE_DIR_EXPECTED}")
        message(FATAL_ERROR
            "fetch_archive_make_available() for ${CONTENT_NAME} source dir paths differ:\n"
            "${SOURCE_DIR_ACTUAL}\n"
            "${SOURCE_DIR_EXPECTED}"
        )
    endif()

    if(NOT FETCHCONTENT_QUIET)
        message(STATUS "Creating archive in: ${${CONTENT_NAME_LOWER}_ARCHIVE_FILENAME}")
    endif()

    # CMake 3.18 adds a file(ARCHIVE_CREATE ...) option for making a file archive, but there's not yet an option to set the working directory.
    # This method with execute_process can be used instead.
    file(MAKE_DIRECTORY "${PROJECT_SOURCE_DIR}/extern")
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar "c${ARCHIVE_OPTIONS}" "${${CONTENT_NAME_LOWER}_ARCHIVE_FILENAME}" --format=zip "."
        WORKING_DIRECTORY "${SOURCE_DIR_ACTUAL}"
    )
endfunction()

macro(fetch_archive_make_available)
    foreach(_CONTENT_NAME IN ITEMS ${ARGV})
        _pre_fetch_archive_make_available(${_CONTENT_NAME})
        FetchContent_MakeAvailable(${_CONTENT_NAME})
        _post_fetch_archive_make_available(${_CONTENT_NAME})
    endforeach()
endmacro()
