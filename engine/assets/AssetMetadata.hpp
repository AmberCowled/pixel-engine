#pragma once

#include <engine/core/UUID.hpp>
#include <string>

namespace PixelEngine {

    enum class AssetType {
        None = 0,
        Texture,
        Audio,
        Shader,
        Scene
    };

    struct AssetMetadata {
        UUID ID;
        AssetType Type = AssetType::None;
        std::string SourcePath; // Relative to assets root
    };

}
