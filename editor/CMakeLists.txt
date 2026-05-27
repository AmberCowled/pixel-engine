# Editor Library CMake Config
cmake_minimum_required(VERSION 3.20)

add_library(editor STATIC
    EditorContext.hpp
    EditorPanel.hpp
    EditorIcons.hpp
    EditorUtils.hpp
    EditorCommands.hpp
    
    panels/AnimationEditorPanel.hpp
    panels/AnimationEditorPanel.cpp
    panels/AssetBrowserPanel.hpp
    panels/AssetBrowserPanel.cpp
    panels/AudioMixerPanel.hpp
    panels/AudioMixerPanel.cpp
    panels/ConsolePanel.hpp
    panels/ConsolePanel.cpp
    panels/HierarchyPanel.hpp
    panels/HierarchyPanel.cpp
    panels/InspectorPanel.hpp
    panels/InspectorPanel.cpp
    panels/ProfilerPanel.hpp
    panels/ProfilerPanel.cpp
    panels/ProjectLauncherPanel.hpp
    panels/ProjectLauncherPanel.cpp
    panels/TilemapPaintingPanel.hpp
    panels/TilemapPaintingPanel.cpp
    panels/ToolbarPanel.hpp
    panels/ToolbarPanel.cpp
    panels/ViewportPanel.hpp
    panels/ViewportPanel.cpp
)

target_link_libraries(editor PUBLIC 
    engine-core 
    engine-renderer 
    engine-ecs 
    engine-assets 
    engine-audio 
    engine-scripting
)

target_include_directories(editor PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
