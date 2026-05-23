#pragma once

#include <glm/glm.hpp>

namespace PixelEngine {

    struct GlobalUBO {
        glm::mat4 view;
        glm::mat4 proj;
        glm::vec2 resolution;
        float pixelSnapping;
    };

    struct PushConstantData {
        glm::mat4 model{1.0f};
        glm::vec4 color{1.0f};
    };

}
