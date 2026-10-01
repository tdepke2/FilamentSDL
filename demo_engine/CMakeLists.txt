set(RESOURCE_BINS "")

filament_compile_materials(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/material"
    OUTPUT_VAR "RESOURCE_BINS"
    PROG_FLAGS -a vulkan -p desktop
    SOURCES
    materials/texturedLit.mat
)

filament_resource_gen(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/resources"
    OUTPUT_VAR "RESGEN_SOURCE"
    ARCHIVE_NAME "resources"
    RESOURCES ${RESOURCE_BINS}
)

add_executable(demo_engine
    main.cpp
    StbUsage.cpp
    ${RESGEN_SOURCE}
)

# Specify where to find the header files to include.
target_include_directories(demo_engine PUBLIC .)

target_include_directories(demo_engine PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# Link to libraries and set C++ compilation version.
target_link_libraries(demo_engine PRIVATE
    fsdl_filament_app
    Filament::Basis_transcoder
    Filament::Camutils
    Filament::Dracodec
    Filament::Filamat
    Filament::Geometry
    Filament::Gltfio
    Filament::Gltfio_core
    Filament::Ktxreader
    Filament::Shaders
    Filament::Uberzlib
)
fsdl_add_cxx_properties(demo_engine)
