find_package(Vulkan REQUIRED COMPONENTS glslc)

set(SHADER_SOURCES
    simple.vert
    simple.frag
    upscale.vert
    upscale.frag
)

set(SPV_FILES "")

# Ensure the output directory exists in the build folder
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/bin/shaders")

foreach(SHADER ${SHADER_SOURCES})
    set(SOURCE_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${SHADER}")
    set(SPV_FILE "${CMAKE_BINARY_DIR}/bin/shaders/${SHADER}.spv")
    
    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND Vulkan::glslc "${SOURCE_FILE}" -o "${SPV_FILE}"
        DEPENDS "${SOURCE_FILE}"
        COMMENT "Compiling shader ${SHADER} to ${SPV_FILE}"
    )
    list(APPEND SPV_FILES ${SPV_FILE})
endforeach()

add_custom_target(compiled_shaders ALL DEPENDS ${SPV_FILES})
