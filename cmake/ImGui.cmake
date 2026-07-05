# Dear ImGui does not provide CMake scripts for compiling its sources, so create
# a target for it. Note that we do not select a backend as filament_app will
# provide this.

add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imconfig.h"
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui.h"

    "${imgui_SOURCE_DIR}/imgui_internal.h"
    "${imgui_SOURCE_DIR}/imstb_rectpack.h"
    "${imgui_SOURCE_DIR}/imstb_textedit.h"
    "${imgui_SOURCE_DIR}/imstb_truetype.h"

    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h"
)
add_library(imgui::imgui ALIAS imgui)

target_include_directories(imgui PUBLIC "${imgui_SOURCE_DIR}")

target_compile_features(imgui PUBLIC cxx_std_20)
set_target_properties(imgui PROPERTIES CXX_EXTENSIONS OFF)
