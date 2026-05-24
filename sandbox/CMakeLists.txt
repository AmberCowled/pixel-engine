# Sandbox Application
add_executable(sandbox main.cpp)
target_link_libraries(sandbox PRIVATE engine-core engine-renderer engine-ecs engine-assets engine-audio)

# Copy SDL3.dll to binary directory on Windows
if (WIN32)
    add_custom_command(TARGET sandbox POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3::SDL3>
        $<TARGET_FILE_DIR:sandbox>
        COMMENT "Copying SDL3.dll to executable directory"
    )
endif()
