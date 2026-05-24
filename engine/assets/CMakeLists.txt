# Engine Assets Library
add_library(engine-assets STATIC 
    AssetManager.hpp
    AssetManager.cpp
    AssetMetadata.hpp
    AudioClip.hpp
    AssetWatcher.hpp
    AssetWatcher.cpp
)
target_include_directories(engine-assets PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}
)
target_link_libraries(engine-assets 
    PUBLIC 
        engine-core
        engine-renderer
        nlohmann_json::nlohmann_json
)
