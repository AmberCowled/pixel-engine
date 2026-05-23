#version 450

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 color = texture(texSampler, inUV);
    
    // Simple Posterization (Optional Polish)
    float levels = 8.0;
    color.rgb = floor(color.rgb * levels) / levels;
    
    outColor = color;
}
