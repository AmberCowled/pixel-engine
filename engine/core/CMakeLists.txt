# Engine Core Library
add_library(engine-core STATIC 
    EngineApp.hpp
    EngineApp.cpp
    UUID.hpp
    UUID.cpp
    EditorHistory.hpp
)
target_include_directories(engine-core PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(engine-core 
    PUBLIC 
        engine-base
        engine-renderer
        SDL3::SDL3
)
