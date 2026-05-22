# Engine Base Library (Logging, etc.)
add_library(engine-base STATIC 
    Log.hpp
    Log.cpp
)
target_include_directories(engine-base PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(engine-base PUBLIC spdlog::spdlog)
