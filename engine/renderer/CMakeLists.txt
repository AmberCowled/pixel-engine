# Engine Renderer Library
add_library(engine-renderer STATIC
    VulkanContext.hpp
    VulkanContext.cpp
    Buffer.hpp
    Buffer.cpp
    Vertex.hpp
    UBO.hpp
    GraphicsPipeline.hpp
    GraphicsPipeline.cpp
    Camera.hpp
    Camera.cpp
    OffscreenTarget.hpp
    OffscreenTarget.cpp
    Texture.hpp
    Texture.cpp
    
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
    ${CMAKE_SOURCE_DIR} # Added project root for global engine/ includes
    ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(engine-renderer 
    PUBLIC 
        Vulkan::Vulkan 
        glm::glm
        engine-base
    PRIVATE
        SDL3::SDL3
)
