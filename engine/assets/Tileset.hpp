#pragma once

#include <engine/core/UUID.hpp>
#include <map>

namespace PixelEngine {

    struct Tileset {
        UUID ID = 0;
        UUID TextureID = 0;
        uint32_t TileSize = 16;
        std::map<uint32_t, bool> SolidTiles; // tileIndex -> isSolid
    };

}
