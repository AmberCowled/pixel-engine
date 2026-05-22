#pragma once

#include "Vertex.hpp"
#include <vector>

namespace PixelEngine {

    const std::vector<Vertex> FULLSCREEN_QUAD_VERTICES = {
        {{-1.0f, -1.0f, 0.0f}, {1,1,1}, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, 0.0f}, {1,1,1}, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, 0.0f}, {1,1,1}, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, 0.0f}, {1,1,1}, {0.0f, 1.0f}}
    };

    const std::vector<uint16_t> FULLSCREEN_QUAD_INDICES = {
        0, 3, 2, 2, 1, 0  // CCW: TL -> BL -> BR, BR -> TR -> TL
    };

}
