set(RESOURCE_BINS "")

filament_compile_materials(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/material"
    OUTPUT_VAR "RESOURCE_BINS"
    PROG_FLAGS -a vulkan -p desktop
    SOURCES
    materials/guiBlit.mat
)

filament_resource_gen(
    OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/resources"
    OUTPUT_VAR "RESGEN_SOURCE"
    ARCHIVE_NAME "filament_app"
    RESOURCES ${RESOURCE_BINS}
)

set(FSDL_FILAMENT_APP_HEADERS
    App.h
    Gui.h
)
list(TRANSFORM FSDL_FILAMENT_APP_HEADERS PREPEND "${PROJECT_SOURCE_DIR}/include/filament_app/")

add_library(fsdl_filament_app
    App.cpp
    Gui.cpp
    ${FSDL_FILAMENT_APP_HEADERS}
    ${RESGEN_SOURCE}
)

source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${FSDL_FILAMENT_APP_HEADERS}
)

target_include_directories(fsdl_filament_app PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

target_include_directories(fsdl_filament_app PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(fsdl_filament_app PUBLIC
    Filament::Backend
    Filament::Bluegl
    Filament::Bluevk
    Filament::Filabridge
    Filament::Filaflat
    Filament::Filament
    Filament::Smol-v
    Filament::Utils
    Filament::Zstd
    SDL3::SDL3
    imgui::imgui
)
fsdl_add_cxx_properties(fsdl_filament_app)
