#pragma once

#include <glm/glm.hpp>

namespace PixelEngine {

    struct GlobalUBO {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    };

}
