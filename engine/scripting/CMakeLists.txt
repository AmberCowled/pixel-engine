# C# Scripting Integration Library
add_library(engine-scripting STATIC
    ScriptEngine.hpp
    ScriptEngine.cpp
)

target_include_directories(engine-scripting PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)

target_link_libraries(engine-scripting
    PUBLIC
        engine-ecs
)
