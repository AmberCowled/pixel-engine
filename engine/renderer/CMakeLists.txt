# Engine Renderer Library
add_library(engine-renderer STATIC
    VulkanContext.hpp
    VulkanContext.cpp
)
target_include_directories(engine-renderer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
