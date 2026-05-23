#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D texSampler;

void main() {
    vec4 texColor = texture(texSampler, fragUV);
    if (texColor.a < 0.001) discard; // Support lower alpha limits if desired, or keep 0.1. Let's discard only absolute transparent fragments for compatibility.
    outColor = texColor * fragColor;
}
