
# Create the main library from all source files.
add_library(fsdl_src
    Common.cpp
    Common.h
)

# Specify where to find the header files to include.
target_include_directories(fsdl_src PUBLIC .)

# Link to libraries and set C++ compilation version.
target_link_libraries(fsdl_src PUBLIC
    Filament::FilamentAll
    #spdlog::spdlog
)
fsdl_add_cxx_properties(fsdl_src)

add_executable(fsdl_demo
    main.cpp
)
target_link_libraries(fsdl_demo PRIVATE fsdl_src)
fsdl_add_cxx_properties(fsdl_demo)
