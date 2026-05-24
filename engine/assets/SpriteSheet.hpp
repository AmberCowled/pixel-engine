#pragma once

#include <engine/core/UUID.hpp>
#include <glm/glm.hpp>
#include <array>
#include <unordered_map>
#include <string>

namespace PixelEngine {

    struct SpriteSheetFrame {
        std::array<glm::vec2, 4> UVs = {
            glm::vec2{ 0.0f, 0.0f },
            glm::vec2{ 1.0f, 0.0f },
            glm::vec2{ 1.0f, 1.0f },
            glm::vec2{ 0.0f, 1.0f }
        };
    };

    struct SpriteSheet {
        UUID ID = 0;
        UUID TextureID = 0;
        std::unordered_map<std::string, SpriteSheetFrame> Frames;
    };

}
