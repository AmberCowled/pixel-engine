#include "AnimationEditorPanel.hpp"
#include <imgui.h>
#include <engine/assets/AssetManager.hpp>
#include <engine/ecs/Components.hpp>
#include <cstring>

namespace PixelEngine {

    void AnimationEditorPanel::OnImGuiRender() {
        ImGui::Begin("Animation Editor");
        if (m_Context.SelectedEntity && m_Context.SelectedEntity.HasComponent<AnimatorComponent>()) {
            auto& ac = m_Context.SelectedEntity.GetComponent<AnimatorComponent>();
            
            // 1. Spritesheet selection
            ImGui::Text("Select Sprite Sheet:");
            for (const auto& [uuid, meta] : AssetManager::GetMetadataRegistry()) {
                if (meta.Type == AssetType::SpriteSheet) {
                    bool isSelected = (ac.SpriteSheetID == uuid);
                    if (ImGui::Selectable(meta.SourcePath.c_str(), isSelected)) {
                        ac.SpriteSheetID = uuid;
                    }
                }
            }
            
            ImGui::Separator();
            
            auto spritesheet = AssetManager::GetSpriteSheet(ac.SpriteSheetID);
            if (spritesheet) {
                // 2. Clip controls
                ImGui::Text("Clips:");
                static char newClipBuffer[64] = "";
                ImGui::InputText("New Clip Name", newClipBuffer, sizeof(newClipBuffer));
                ImGui::SameLine();
                if (ImGui::Button("Add Clip") && strlen(newClipBuffer) > 0) {
                    AnimationClip newClip;
                    newClip.Name = newClipBuffer;
                    newClip.FPS = 10.0f;
                    newClip.Loop = true;
                    ac.Clips.push_back(newClip);
                    if (ac.CurrentClip.empty()) {
                        ac.CurrentClip = newClip.Name;
                    }
                    newClipBuffer[0] = '\0';
                }
                
                int clipToDelete = -1;
                for (int i = 0; i < static_cast<int>(ac.Clips.size()); i++) {
                    auto& clip = ac.Clips[i];
                    bool isCurrent = (ac.CurrentClip == clip.Name);
                    
                    ImGui::PushID(i);
                    if (ImGui::Selectable(clip.Name.c_str(), isCurrent)) {
                        ac.CurrentClip = clip.Name;
                        ac.CurrentFrame = 0;
                        ac.Timer = 0.0f;
                    }
                    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60.0f);
                    if (ImGui::Button("Delete")) {
                        clipToDelete = i;
                    }
                    ImGui::PopID();
                }
                
                if (clipToDelete >= 0) {
                    std::string deletedName = ac.Clips[clipToDelete].Name;
                    ac.Clips.erase(ac.Clips.begin() + clipToDelete);
                    if (ac.CurrentClip == deletedName) {
                        ac.CurrentClip = ac.Clips.empty() ? "" : ac.Clips[0].Name;
                        ac.CurrentFrame = 0;
                        ac.Timer = 0.0f;
                    }
                }
                
                ImGui::Separator();
                
                // 3. Edit current clip
                AnimationClip* currentClip = nullptr;
                for (auto& c : ac.Clips) {
                    if (c.Name == ac.CurrentClip) {
                        currentClip = &c;
                        break;
                    }
                }
                
                if (currentClip) {
                    ImGui::Text("Editing Clip: %s", currentClip->Name.c_str());
                    
                    ImGui::SliderFloat("FPS", &currentClip->FPS, 1.0f, 60.0f);
                    ImGui::Checkbox("Loop", &currentClip->Loop);
                    
                    // Display preview/playback controls
                    ImGui::Checkbox("Play Preview", &ac.Playing);
                    
                    ImGui::Text("Timeline Frames (Count: %d):", (int)currentClip->Frames.size());
                    
                    // List frames in current clip
                    int frameToMoveUp = -1;
                    int frameToMoveDown = -1;
                    int frameToRemove = -1;
                    
                    for (int i = 0; i < static_cast<int>(currentClip->Frames.size()); i++) {
                        auto& frame = currentClip->Frames[i];
                        ImGui::PushID(i);
                        
                        ImGui::Text("[%d] Frame: %s", i, frame.FrameName.c_str());
                        ImGui::SameLine();
                        
                        char eventBuf[64];
                        strncpy_s(eventBuf, frame.EventName.c_str(), sizeof(eventBuf));
                        ImGui::SetNextItemWidth(100.0f);
                        if (ImGui::InputText("Event", eventBuf, sizeof(eventBuf))) {
                            frame.EventName = eventBuf;
                        }
                        
                        ImGui::SameLine();
                        if (ImGui::Button("^") && i > 0) {
                            frameToMoveUp = i;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("v") && i < static_cast<int>(currentClip->Frames.size()) - 1) {
                            frameToMoveDown = i;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("X")) {
                            frameToRemove = i;
                        }
                        
                        ImGui::PopID();
                    }
                    
                    if (frameToMoveUp >= 0) {
                        std::swap(currentClip->Frames[frameToMoveUp], currentClip->Frames[frameToMoveUp - 1]);
                    }
                    if (frameToMoveDown >= 0) {
                        std::swap(currentClip->Frames[frameToMoveDown], currentClip->Frames[frameToMoveDown + 1]);
                    }
                    if (frameToRemove >= 0) {
                        currentClip->Frames.erase(currentClip->Frames.begin() + frameToRemove);
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("Add Frame from Sprite Sheet:");
                    
                    // Show available frames from the spritesheet
                    for (const auto& [frameName, frameData] : spritesheet->Frames) {
                        if (ImGui::Button(frameName.c_str())) {
                            AnimationFrame newFrame;
                            newFrame.FrameName = frameName;
                            newFrame.EventName = "";
                            currentClip->Frames.push_back(newFrame);
                        }
                    }
                }
            } else {
                ImGui::Text("No Sprite Sheet loaded on this animator.");
            }
        } else {
            ImGui::Text("Select an entity with an Animator Component to edit animations.");
        }
        ImGui::End();
    }

}
