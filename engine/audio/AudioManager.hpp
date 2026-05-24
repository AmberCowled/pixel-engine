#pragma once

#include <string>

namespace PixelEngine {

    class AudioManager {
    public:
        static void Init();
        static void Shutdown();

        static float GetMasterVolume() { return s_MasterVolume; }
        static void SetMasterVolume(float volume) { s_MasterVolume = volume; }

        static float GetMusicVolume() { return s_MusicVolume; }
        static void SetMusicVolume(float volume) { s_MusicVolume = volume; }

        static float GetSFXVolume() { return s_SFXVolume; }
        static void SetSFXVolume(float volume) { s_SFXVolume = volume; }

    private:
        static inline float s_MasterVolume = 1.0f;
        static inline float s_MusicVolume = 1.0f;
        static inline float s_SFXVolume = 1.0f;
        static inline bool s_Initialized = false;
    };

}
