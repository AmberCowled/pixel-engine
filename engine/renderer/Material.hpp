#pragma once

#include <engine/core/UUID.hpp>
#include <glm/glm.hpp>
#include <string>

namespace PixelEngine {

    enum class BlendMode {
        Opaque = 0,
        AlphaBlend,
        Additive
    };

    struct Material {
        std::string ShaderName = "sprite";
        UUID TextureID = 0;
        glm::vec4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        BlendMode Blend = BlendMode::AlphaBlend;
    };

}
