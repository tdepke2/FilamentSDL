set(RESOURCE_BINS "")

filament_compile_materials(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/material"
    OUTPUT_VAR "RESOURCE_BINS"
    PROG_FLAGS -a vulkan -p desktop
    SOURCES
    materials/texturedLit.mat
)

filament_compile_meshes(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/mesh"
    OUTPUT_VAR "RESOURCE_BINS"
    SOURCES
    models/monkey/monkey.obj
)

filament_compile_textures(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/texture"
    OUTPUT_VAR "RESOURCE_BINS"
    PROG_FLAGS --quiet --compression=uastc
    FILE_EXT ".ktx2"
    SOURCES
    models/monkey/albedo.png
)
filament_compile_textures(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/texture"
    OUTPUT_VAR "RESOURCE_BINS"
    PROG_FLAGS --quiet --compression=uastc --grayscale --linear
    FILE_EXT ".ktx2"
    SOURCES
    models/monkey/ao.png
    models/monkey/metallic.png
    models/monkey/roughness.png
)
list(APPEND RESOURCE_BINS "${CMAKE_CURRENT_SOURCE_DIR}/models/monkey/normal.png")

list(APPEND RESOURCE_BINS "${CMAKE_CURRENT_SOURCE_DIR}/fonts/Roboto_Medium.ttf")

filament_resource_gen(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/resources"
    OUTPUT_VAR "RESGEN_SOURCE"
    ARCHIVE_NAME "resources"
    RESOURCES ${RESOURCE_BINS}
)

# Create the main library from all source files.
add_library(fsdl_src
    StbUsage.cpp
    ${RESGEN_SOURCE}
)

# Specify where to find the header files to include.
target_include_directories(fsdl_src PUBLIC .)

# Link to libraries and set C++ compilation version.
target_link_libraries(fsdl_src PUBLIC
    fsdl_filament_app
    Filament::Basis_transcoder
    Filament::Camutils
    Filament::Filament-iblprefilter
    Filament::Filameshio
    Filament::Geometry
    Filament::Ktxreader
    stb::stb
)
fsdl_add_cxx_properties(fsdl_src)

add_executable(fsdl_demo
    main.cpp
)
target_include_directories(fsdl_demo PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(fsdl_demo PRIVATE fsdl_src)
fsdl_add_cxx_properties(fsdl_demo)

add_subdirectory(filament_app)
