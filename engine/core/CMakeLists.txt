# Engine Core Library
add_library(engine-core STATIC 
    Log.hpp
    Log.cpp
    EngineApp.hpp
    EngineApp.cpp
)
target_include_directories(engine-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine-core PUBLIC SDL3::SDL3 spdlog::spdlog)
