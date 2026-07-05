set(FSDL_FILAMENT_APP_HEADERS
    App.h
    Gui.h
)
list(TRANSFORM FSDL_FILAMENT_APP_HEADERS PREPEND "${PROJECT_SOURCE_DIR}/include/filament_app/")

add_library(fsdl_filament_app
    App.cpp
    Gui.cpp
    ${FSDL_FILAMENT_APP_HEADERS}
)

source_group(
    TREE "${PROJECT_SOURCE_DIR}/include"
    PREFIX "Header Files"
    FILES ${FSDL_FILAMENT_APP_HEADERS}
)

target_include_directories(fsdl_filament_app PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(fsdl_filament_app PUBLIC
    Filament::FilamentAll
    SDL3::SDL3
    imgui::imgui
)
fsdl_add_cxx_properties(fsdl_filament_app)
