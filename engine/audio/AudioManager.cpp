#include "AudioManager.hpp"
#include <SDL3/SDL.h>
#include <engine/base/Log.hpp>

namespace PixelEngine {

    void AudioManager::Init() {
        if (s_Initialized) return;

        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            PX_CORE_ERROR("AudioManager: Failed to initialize SDL Audio: {0}", SDL_GetError());
            return;
        }

        s_Initialized = true;
        PX_CORE_INFO("AudioManager: SDL Audio initialized successfully.");
    }

    void AudioManager::Shutdown() {
        if (!s_Initialized) return;

        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        s_Initialized = false;
        PX_CORE_INFO("AudioManager: SDL Audio shut down.");
    }

}
