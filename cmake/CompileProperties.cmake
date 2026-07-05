
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries.")

# Set a required c++ version and strict warnings for a target.
function(fsdl_add_cxx_properties TARGET_NAME)
    target_compile_features(${TARGET_NAME} PUBLIC cxx_std_20)
    set_target_properties(${TARGET_NAME} PROPERTIES CXX_EXTENSIONS OFF)

    set_target_properties(${TARGET_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    )

    # Enable strict warnings. Could also use CMake presets here for better compiler support:
    # https://alexreinking.com/blog/how-to-use-cmake-without-the-agonizing-pain-part-2.html
    target_compile_options(${TARGET_NAME} PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/W4>
        $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
    )
endfunction()
