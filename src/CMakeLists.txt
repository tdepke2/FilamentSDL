
# Create the main library from all source files.
add_library(fsdl_src
    Common.cpp
    Common.h
    ../gen/res/filagui_resources.c
)

# .\extern\filament-v1.72.0-windows\bin\matc.exe -a vulkan -p desktop -o .\gen\guiBlit.filamat .\src\filament_app\materials\guiBlit.mat
# .\extern\filament-v1.72.0-windows\bin\resgen.exe -cx .\gen\res\ -p filagui_resources .\gen\guiBlit.filamat

# Specify where to find the header files to include.
target_include_directories(fsdl_src PUBLIC .)

# Link to libraries and set C++ compilation version.
target_link_libraries(fsdl_src PUBLIC
    fsdl_filament_app
)
fsdl_add_cxx_properties(fsdl_src)

add_executable(fsdl_demo
    main.cpp
)
target_link_libraries(fsdl_demo PRIVATE fsdl_src)
fsdl_add_cxx_properties(fsdl_demo)

add_subdirectory(filament_app)
