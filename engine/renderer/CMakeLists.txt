# Engine Renderer Library
add_library(engine-renderer STATIC
    VulkanContext.hpp
    VulkanContext.cpp
    
    # ImGui Core
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    
    # ImGui Backends
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(engine-renderer PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(engine-renderer 
    PUBLIC 
        Vulkan::Vulkan 
        glm::glm
    PRIVATE
        engine-core # For SDL3 headers and logging
)
