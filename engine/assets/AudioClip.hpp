#pragma once

#include <engine/core/UUID.hpp>
#include <SDL3/SDL.h>
#include <string>

namespace PixelEngine {

    struct AudioClip {
        UUID ID = 0;
        std::string Path = "";
        uint8_t* Buffer = nullptr;
        uint32_t Length = 0;
        SDL_AudioSpec Spec;

        ~AudioClip() {
            if (Buffer) {
                SDL_free(Buffer);
            }
        }
    };

}
