#version 450

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec2 resolution;
    float pixelSnapping;
    float padding;
    vec4 baseColor;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 fragUV;

void main() {
    vec4 position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // Pixel Snapping: Convert to NDC, snap to grid, and convert back
    if (ubo.pixelSnapping > 0.5 && ubo.resolution.x > 0 && ubo.resolution.y > 0) {
        vec2 gridPos = (position.xy / position.w);
        gridPos = round(gridPos * ubo.resolution * 0.5) / (ubo.resolution * 0.5);
        position.xy = gridPos * position.w;
    }

    gl_Position = position;
    fragColor = inColor * ubo.baseColor.rgb;
    fragUV = inUV;
}
