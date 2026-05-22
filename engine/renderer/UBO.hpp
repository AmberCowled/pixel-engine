#pragma once

#include <glm/glm.hpp>

namespace PixelEngine {

    struct GlobalUBO {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec2 resolution;
        float pixelSnapping; // 0.0 for off, 1.0 for on
        float padding;
        glm::vec4 baseColor;
    };

}
