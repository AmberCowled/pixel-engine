# Engine ECS Library
add_library(engine-ecs STATIC 
    Scene.hpp
    Scene.cpp
    Entity.hpp
    Components.hpp
    RenderSystem.hpp
    RenderSystem.cpp
)
target_include_directories(engine-ecs PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(engine-ecs 
    PUBLIC 
        engine-core
        EnTT::EnTT
        engine-assets
)
