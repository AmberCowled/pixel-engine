# Sandbox Application
add_executable(sandbox main.cpp)
target_link_libraries(sandbox PRIVATE engine-core engine-renderer engine-ecs engine-assets engine-audio engine-scripting)

# Copy SDL3.dll and PixelEngineScripting.dll to binary directory on Windows
if (WIN32)
    add_custom_command(TARGET sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:sandbox>
        COMMENT "Copying SDL3.dll to executable directory"
    )
    add_custom_command(TARGET sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/engine/scripting/PixelEngineScripting/bin/Release/net8.0/PixelEngineScripting.dll"
        $<TARGET_FILE_DIR:sandbox>
        COMMENT "Copying PixelEngineScripting.dll to executable directory"
    )
endif()
