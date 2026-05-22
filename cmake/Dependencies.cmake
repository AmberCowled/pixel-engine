# Dependency Management with CPM
include(cmake/CPM.cmake)

# SDL3 - Windowing and Input
CPMAddPackage(
    NAME SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main # SDL3 is under active development
    OPTIONS
        "SDL_SHARED ON"
        "SDL_STATIC OFF"
        "SDL_TEST OFF"
)

# GLM - Mathematics
CPMAddPackage(
    NAME glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

# spdlog - Logging
CPMAddPackage(
    NAME spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.12.0
)

# Dear ImGui - Debug UI
# Note: ImGui doesn't have a standard CMakeLists.txt for library use, 
# so we fetch it and will add its sources to our renderer library.
CPMAddPackage(
    NAME imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
    DOWNLOAD_ONLY YES
)

# Vulkan SDK
find_package(Vulkan REQUIRED)
