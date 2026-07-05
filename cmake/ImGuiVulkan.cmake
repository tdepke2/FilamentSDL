# Dear ImGui does not provide CMake scripts for compiling its sources or the
# Vulkan backend, so create a target for it.

add_library(imgui-vulkan STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui.h"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.h"

    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.h"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.h"
)
add_library(imgui-vulkan::imgui-vulkan ALIAS imgui-vulkan)

target_include_directories(imgui-vulkan PUBLIC "${imgui_SOURCE_DIR}")
target_link_libraries(imgui-vulkan PUBLIC Vulkan::Vulkan SDL3::SDL3)

target_compile_features(imgui-vulkan PUBLIC cxx_std_20)
set_target_properties(imgui-vulkan PROPERTIES CXX_EXTENSIONS OFF)
