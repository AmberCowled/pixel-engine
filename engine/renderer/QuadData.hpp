#pragma once

#include "Vertex.hpp"
#include <vector>

namespace PixelEngine {

    // A single large triangle that covers the entire NDC space (-1 to 1)
    // and provides UVs from 0 to 2 (effectively 0 to 1 over the screen area)
    const std::vector<Vertex> FULLSCREEN_TRIANGLE_VERTICES = {
        {{-1.0f, -1.0f, 0.0f}, {1,1,1}, {0.0f, 0.0f}},
        {{ 3.0f, -1.0f, 0.0f}, {1,1,1}, {2.0f, 0.0f}},
        {{-1.0f,  3.0f, 0.0f}, {1,1,1}, {0.0f, 2.0f}}
    };

    const std::vector<uint16_t> FULLSCREEN_TRIANGLE_INDICES = { 0, 1, 2 };

    const std::vector<Vertex> SPRITE_VERTICES = {
        {{-0.5f, -0.5f, 0.0f}, {1,1,1}, {0.0f, 0.0f}},
        {{ 0.5f, -0.5f, 0.0f}, {1,1,1}, {1.0f, 0.0f}},
        {{ 0.5f,  0.5f, 0.0f}, {1,1,1}, {1.0f, 1.0f}},
        {{-0.5f,  0.5f, 0.0f}, {1,1,1}, {0.0f, 1.0f}}
    };

    const std::vector<uint16_t> SPRITE_INDICES = { 0, 1, 2, 2, 3, 0 };

}
