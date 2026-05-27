#include "AudioMixerPanel.hpp"
#include <imgui.h>
#include <engine/audio/AudioManager.hpp>

namespace PixelEngine {

    void AudioMixerPanel::OnImGuiRender() {
        ImGui::Begin("Audio Mixer");
        {
            float masterVol = AudioManager::GetMasterVolume();
            if (ImGui::SliderFloat("Master Volume", &masterVol, 0.0f, 1.0f)) {
                AudioManager::SetMasterVolume(masterVol);
            }

            float musicVol = AudioManager::GetMusicVolume();
            if (ImGui::SliderFloat("Music Volume", &musicVol, 0.0f, 1.0f)) {
                AudioManager::SetMusicVolume(musicVol);
            }

            float sfxVol = AudioManager::GetSFXVolume();
            if (ImGui::SliderFloat("SFX Volume", &sfxVol, 0.0f, 1.0f)) {
                AudioManager::SetSFXVolume(sfxVol);
            }
        }
        ImGui::End();
    }

}
