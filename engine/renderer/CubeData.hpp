#pragma once

#include "Vertex.hpp"
#include <vector>

namespace PixelEngine {

    const std::vector<Vertex> CUBE_VERTICES = {
        // Front face (+z) - Pink
        {{-.5f, -.5f,  .5f}, {.9f, .6f, .7f}}, // 0: BL
        {{ .5f, -.5f,  .5f}, {.9f, .6f, .7f}}, // 1: BR
        {{ .5f,  .5f,  .5f}, {.9f, .6f, .7f}}, // 2: TR
        {{-.5f,  .5f,  .5f}, {.9f, .6f, .7f}}, // 3: TL

        // Back face (-z) - Green
        {{ .5f, -.5f, -.5f}, {.1f, .8f, .1f}}, // 4: BL (looking from -z)
        {{-.5f, -.5f, -.5f}, {.1f, .8f, .1f}}, // 5: BR
        {{-.5f,  .5f, -.5f}, {.1f, .8f, .1f}}, // 6: TR
        {{ .5f,  .5f, -.5f}, {.1f, .8f, .1f}}, // 7: TL

        // Left face (-x) - White
        {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}}, // 8: BL (looking from -x)
        {{-.5f, -.5f,  .5f}, {.9f, .9f, .9f}}, // 9: BR
        {{-.5f,  .5f,  .5f}, {.9f, .9f, .9f}}, // 10: TR
        {{-.5f,  .5f, -.5f}, {.9f, .9f, .9f}}, // 11: TL

        // Right face (+x) - Gold
        {{ .5f, -.5f,  .5f}, {.8f, .8f, .1f}}, // 12: BL (looking from +x)
        {{ .5f, -.5f, -.5f}, {.8f, .8f, .1f}}, // 13: BR
        {{ .5f,  .5f, -.5f}, {.8f, .8f, .1f}}, // 14: TR
        {{ .5f,  .5f,  .5f}, {.8f, .8f, .1f}}, // 15: TL

        // Top face (-y) - Blue (Vulkan -y is up)
        {{-.5f, -.5f, -.5f}, {.1f, .1f, .8f}}, // 16: BL (looking from -y)
        {{ .5f, -.5f, -.5f}, {.1f, .1f, .8f}}, // 17: BR
        {{ .5f, -.5f,  .5f}, {.1f, .1f, .8f}}, // 18: TR
        {{-.5f, -.5f,  .5f}, {.1f, .1f, .8f}}, // 19: TL

        // Bottom face (+y) - Red
        {{-.5f,  .5f,  .5f}, {.8f, .1f, .1f}}, // 20: BL (looking from +y)
        {{ .5f,  .5f,  .5f}, {.8f, .1f, .1f}}, // 21: BR
        {{ .5f,  .5f, -.5f}, {.8f, .1f, .1f}}, // 22: TR
        {{-.5f,  .5f, -.5f}, {.8f, .1f, .1f}}, // 23: TL
    };

    const std::vector<uint16_t> CUBE_INDICES = {
        0,  1,  2,  0,  2,  3,  // Front
        4,  5,  6,  4,  6,  7,  // Back
        8,  9,  10, 8,  10, 11, // Left
        12, 13, 14, 12, 14, 15, // Right
        16, 17, 18, 16, 18, 19, // Top
        20, 21, 22, 20, 22, 23  // Bottom
    };

}
