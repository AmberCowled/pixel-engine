# Engine Audio Library
add_library(engine-audio STATIC 
    AudioManager.hpp
    AudioManager.cpp
)
target_include_directories(engine-audio PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(engine-audio 
    PUBLIC 
        engine-core
        SDL3::SDL3
)
