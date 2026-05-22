# Engine Core Library
add_library(engine-core STATIC 
    Log.hpp
    Log.cpp
)
target_include_directories(engine-core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
