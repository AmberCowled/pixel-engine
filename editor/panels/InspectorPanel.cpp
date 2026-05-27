#include "InspectorPanel.hpp"
#include <imgui.h>
#include <SDL3/SDL.h>
#include "editor/EditorIcons.hpp"
#include "editor/EditorContext.hpp"
#include "editor/EditorUtils.hpp"
#include "editor/EditorCommands.hpp"
#include <engine/ecs/SceneSerializer.hpp>
#include <engine/core/EditorHistory.hpp>
#include <engine/ecs/Components.hpp>
#include <engine/assets/AssetManager.hpp>
#include <engine/base/Log.hpp>
#include <algorithm>
#include <cstring>

namespace PixelEngine {

    void InspectorPanel::OnImGuiRender() {
        ImGui::Begin("Inspector");
        if (m_Context.SelectedEntity) {
            UUID prefabID = 0;
            if (IsPartOfPrefab(m_Context.SelectedEntity, m_Context.ActiveScene.get(), prefabID)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.376f, 0.647f, 0.980f, 1.0f));
                ImGui::Text(ICON_FA_CUBE " Prefab Instance");
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button("Apply Overrides")) {
                    if (m_Context.ApplyPrefabOverridesCallback) {
                        m_Context.ApplyPrefabOverridesCallback(m_Context.SelectedEntity, prefabID);
                    }
                }
                ImGui::SameLine();
                if (ImGui::Button("Revert Overrides")) {
                    if (m_Context.RevertPrefabOverridesCallback) {
                        m_Context.RevertPrefabOverridesCallback(m_Context.SelectedEntity, prefabID);
                    }
                }
                ImGui::Separator();
            }

            auto& tag = m_Context.SelectedEntity.GetComponent<TagComponent>();
            char buffer[256];
            std::memset(buffer, 0, sizeof(buffer));
            std::strncpy(buffer, tag.Tag.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("Tag", buffer, sizeof(buffer))) {
                tag.Tag = std::string(buffer);
                TrackOverride(m_Context.SelectedEntity, "TagComponent.Tag");
            }

            ImGui::Separator();

            if (m_Context.SelectedEntity.HasComponent<TransformComponent>()) {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& tc = m_Context.SelectedEntity.GetComponent<TransformComponent>();
                    
                    static nlohmann::json beforeEditState;
                    static bool isEditing = false;
                    
                    bool changed = false;
                    if (ImGui::DragFloat3("Position", &tc.Translation.x, 0.1f)) {
                        changed = true;
                        TrackOverride(m_Context.SelectedEntity, "TransformComponent.Translation");
                    }
                    if (ImGui::DragFloat3("Rotation", &tc.Rotation.x, 0.1f)) {
                        changed = true;
                        TrackOverride(m_Context.SelectedEntity, "TransformComponent.Rotation");
                    }
                    if (ImGui::DragFloat3("Scale", &tc.Scale.x, 0.1f)) {
                        changed = true;
                        TrackOverride(m_Context.SelectedEntity, "TransformComponent.Scale");
                    }

                    if (changed && !isEditing) {
                        SceneSerializer serializer(*m_Context.ActiveScene);
                        beforeEditState = serializer.SerializeToJson();
                        isEditing = true;
                    }
                    
                    if (isEditing && (ImGui::IsItemDeactivatedAfterEdit() || (!ImGui::IsItemActive() && !ImGui::IsAnyItemActive()))) {
                        SceneSerializer serializer(*m_Context.ActiveScene);
                        nlohmann::json afterEditState = serializer.SerializeToJson();
                        EditorHistory::PushCommand(
                            std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeEditState, afterEditState, "Modify Transform")
                        );
                        isEditing = false;
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<MeshRendererComponent>()) {
                if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& mc = m_Context.SelectedEntity.GetComponent<MeshRendererComponent>();
                    if (ImGui::ColorEdit4("Mesh Color", &mc.Color.x)) {
                        TrackOverride(m_Context.SelectedEntity, "MeshRendererComponent.Color");
                    }
                    
                    ImGui::Text("Texture UUID: %llu", (uint64_t)mc.TextureID);
                    if (mc.TextureID != 0) {
                        ImGui::Text("Path: %s", AssetManager::GetAssetPath(mc.TextureID).c_str());
                    } else {
                        ImGui::Text("Path: None (Default white)");
                    }
                    
                    ImGui::Button("Drag texture here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            UUID uuid = AssetManager::LoadTexture(assetPath);
                            if (uuid != 0) {
                                mc.TextureID = uuid;
                                TrackOverride(m_Context.SelectedEntity, "MeshRendererComponent.TextureID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<SpriteRendererComponent>()) {
                if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& sc = m_Context.SelectedEntity.GetComponent<SpriteRendererComponent>();
                    if (ImGui::ColorEdit4("Sprite Color", &sc.Mat.Color.x)) {
                        TrackOverride(m_Context.SelectedEntity, "SpriteRendererComponent.Color");
                    }

                    const char* blendModes[] = { "Opaque", "AlphaBlend", "Additive" };
                    int currentBlend = static_cast<int>(sc.Mat.Blend);
                    if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, 3)) {
                        sc.Mat.Blend = static_cast<BlendMode>(currentBlend);
                        TrackOverride(m_Context.SelectedEntity, "SpriteRendererComponent.Blend");
                    }
                    
                    ImGui::Text("Texture UUID: %llu", (uint64_t)sc.Mat.TextureID);
                    if (sc.Mat.TextureID != 0) {
                        ImGui::Text("Path: %s", AssetManager::GetAssetPath(sc.Mat.TextureID).c_str());
                    } else {
                        ImGui::Text("Path: None (Default white)");
                    }
                    
                    ImGui::Button("Drag texture here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            UUID uuid = AssetManager::LoadTexture(assetPath);
                            if (uuid != 0) {
                                sc.Mat.TextureID = uuid;
                                TrackOverride(m_Context.SelectedEntity, "SpriteRendererComponent.TextureID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<VelocityComponent>()) {
                if (ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& vc = m_Context.SelectedEntity.GetComponent<VelocityComponent>();
                    if (ImGui::DragFloat3("Linear", &vc.Linear.x, 0.05f)) {
                        TrackOverride(m_Context.SelectedEntity, "VelocityComponent.Linear");
                    }
                    if (ImGui::DragFloat3("Angular", &vc.Angular.x, 0.05f)) {
                        TrackOverride(m_Context.SelectedEntity, "VelocityComponent.Angular");
                    }
                    if (ImGui::Button("Remove Velocity")) {
                        m_Context.SelectedEntity.RemoveComponent<VelocityComponent>();
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<HierarchyComponent>()) {
                if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& hc = m_Context.SelectedEntity.GetComponent<HierarchyComponent>();
                    if (hc.Parent != 0) {
                        auto parentEntity = m_Context.ActiveScene->GetEntityByUUID(hc.Parent);
                        if (parentEntity) {
                            auto& parentTag = parentEntity.GetComponent<TagComponent>();
                            ImGui::Text("Parent: %s", parentTag.Tag.c_str());
                        } else {
                            ImGui::Text("Parent UUID: %llu (not found)", (uint64_t)hc.Parent);
                        }
                        if (ImGui::Button("Clear Parent")) {
                            auto parentEnt = m_Context.ActiveScene->GetEntityByUUID(hc.Parent);
                            if (parentEnt && parentEnt.HasComponent<HierarchyComponent>()) {
                                auto& parentHc = parentEnt.GetComponent<HierarchyComponent>();
                                auto myUUID = m_Context.SelectedEntity.GetComponent<IDComponent>().ID;
                                parentHc.Children.erase(std::remove(parentHc.Children.begin(), parentHc.Children.end(), myUUID), parentHc.Children.end());
                            }
                            hc.Parent = 0;
                        }
                    } else {
                        ImGui::Text("No Parent");
                    }
                    
                    if (ImGui::Button("Remove Hierarchy Component")) {
                        m_Context.SelectedEntity.RemoveComponent<HierarchyComponent>();
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<SpriteAnimationComponent>()) {
                if (ImGui::CollapsingHeader("Sprite Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& ac = m_Context.SelectedEntity.GetComponent<SpriteAnimationComponent>();
                    if (ImGui::Checkbox("Playing", &ac.Playing)) {
                        TrackOverride(m_Context.SelectedEntity, "SpriteAnimationComponent.Playing");
                    }
                    if (ImGui::Checkbox("Loop", &ac.Loop)) {
                        TrackOverride(m_Context.SelectedEntity, "SpriteAnimationComponent.Loop");
                    }
                    if (ImGui::SliderFloat("Frame Duration", &ac.FrameTime, 0.05f, 2.0f)) {
                        TrackOverride(m_Context.SelectedEntity, "SpriteAnimationComponent.FrameTime");
                    }
                    ImGui::Text("Frame Count: %d", (int)ac.Textures.size());
                    
                    if (ImGui::Button("Setup Test Anim (Blink)")) {
                        ac.Textures.clear();
                        ac.Textures.push_back(m_Context.TestTexture);
                        ac.Textures.push_back(0); // none texture ID
                        ac.CurrentFrame = 0;
                        ac.Timer = 0.0f;
                        ac.Playing = true;
                        TrackOverride(m_Context.SelectedEntity, "SpriteAnimationComponent.Textures");
                        TrackOverride(m_Context.SelectedEntity, "SpriteAnimationComponent.Playing");
                    }
                    if (ImGui::Button("Remove Animation")) {
                        m_Context.SelectedEntity.RemoveComponent<SpriteAnimationComponent>();
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<TilemapComponent>()) {
                if (ImGui::CollapsingHeader("Tilemap Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& tc = m_Context.SelectedEntity.GetComponent<TilemapComponent>();
                    ImGui::Text("Tileset UUID: %llu", (uint64_t)tc.TilesetID);
                    if (tc.TilesetID != 0) {
                        ImGui::Text("Path: %s", AssetManager::GetAssetPath(tc.TilesetID).c_str());
                    } else {
                        ImGui::Text("Path: None");
                    }
                    
                    ImGui::Button("Drag tileset here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            UUID uuid = AssetManager::LoadTileset(assetPath);
                            if (uuid != 0) {
                                tc.TilesetID = uuid;
                                TrackOverride(m_Context.SelectedEntity, "TilemapComponent.TilesetID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    int renderLayer = tc.RenderLayer;
                    if (ImGui::DragInt("Render Layer", &renderLayer, 1.0f)) {
                        tc.RenderLayer = renderLayer;
                        TrackOverride(m_Context.SelectedEntity, "TilemapComponent.RenderLayer");
                    }

                    int tileSize = (int)tc.TileSize;
                    if (ImGui::DragInt("Tile Size", &tileSize, 1.0f, 1, 128)) {
                        tc.TileSize = static_cast<uint32_t>(tileSize);
                        TrackOverride(m_Context.SelectedEntity, "TilemapComponent.TileSize");
                    }

                    if (ImGui::Button("Remove Tilemap")) {
                        m_Context.SelectedEntity.RemoveComponent<TilemapComponent>();
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<AnimatorComponent>()) {
                if (ImGui::CollapsingHeader("Animator Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& ac = m_Context.SelectedEntity.GetComponent<AnimatorComponent>();
                    ImGui::Text("Spritesheet UUID: %llu", (uint64_t)ac.SpriteSheetID);
                    if (ac.SpriteSheetID != 0) {
                        ImGui::Text("Path: %s", AssetManager::GetAssetPath(ac.SpriteSheetID).c_str());
                    } else {
                        ImGui::Text("Path: None");
                    }
                    
                    ImGui::Button("Drag spritesheet here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            UUID uuid = AssetManager::LoadSpriteSheet(assetPath);
                            if (uuid != 0) {
                                ac.SpriteSheetID = uuid;
                                TrackOverride(m_Context.SelectedEntity, "AnimatorComponent.SpriteSheetID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (ImGui::Checkbox("Playing", &ac.Playing)) {
                        TrackOverride(m_Context.SelectedEntity, "AnimatorComponent.Playing");
                    }
                    ImGui::Text("Current Clip: %s", ac.CurrentClip.c_str());
                    ImGui::Text("Current Frame: %d", ac.CurrentFrame);

                    if (ImGui::Button("Remove Animator")) {
                        m_Context.SelectedEntity.RemoveComponent<AnimatorComponent>();
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<AudioSourceComponent>()) {
                if (ImGui::CollapsingHeader("Audio Source Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& asc = m_Context.SelectedEntity.GetComponent<AudioSourceComponent>();
                    ImGui::Text("Audio Clip UUID: %llu", (uint64_t)asc.ClipID);
                    if (asc.ClipID != 0) {
                        ImGui::Text("Path: %s", AssetManager::GetAssetPath(asc.ClipID).c_str());
                    } else {
                        ImGui::Text("Path: None");
                    }
                    
                    ImGui::Button("Drag .wav here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            UUID uuid = AssetManager::LoadAudioClip(assetPath);
                            if (uuid != 0) {
                                asc.ClipID = uuid;
                                TrackOverride(m_Context.SelectedEntity, "AudioSourceComponent.ClipID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (ImGui::Checkbox("Looping", &asc.Loop)) {
                        TrackOverride(m_Context.SelectedEntity, "AudioSourceComponent.Loop");
                    }
                    if (ImGui::Checkbox("Play On Start", &asc.PlayOnStart)) {
                        TrackOverride(m_Context.SelectedEntity, "AudioSourceComponent.PlayOnStart");
                    }
                    if (ImGui::Checkbox("Is Music Bus", &asc.IsMusic)) {
                        TrackOverride(m_Context.SelectedEntity, "AudioSourceComponent.IsMusic");
                    }

                    float volume = asc.Volume;
                    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
                        asc.Volume = volume;
                        TrackOverride(m_Context.SelectedEntity, "AudioSourceComponent.Volume");
                    }

                    ImGui::Separator();
                    ImGui::Text("Editor Preview Controls:");
                    if (ImGui::Button("Play Preview")) {
                        asc.IsPlaying = true;
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Stop Preview")) {
                        asc.IsPlaying = false;
                        if (asc.Stream) {
                            SDL_DestroyAudioStream(asc.Stream);
                            asc.Stream = nullptr;
                        }
                    }

                    if (ImGui::Button("Remove Audio Source")) {
                        if (asc.Stream) {
                            SDL_DestroyAudioStream(asc.Stream);
                            asc.Stream = nullptr;
                        }
                        m_Context.SelectedEntity.RemoveComponent<AudioSourceComponent>();
                    }
                }
            }

            if (m_Context.SelectedEntity.HasComponent<ScriptComponent>()) {
                if (ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& sc = m_Context.SelectedEntity.GetComponent<ScriptComponent>();
                    char scriptBuffer[256];
                    std::memset(scriptBuffer, 0, sizeof(scriptBuffer));
                    std::strncpy(scriptBuffer, sc.ClassName.c_str(), sizeof(scriptBuffer) - 1);
                    if (ImGui::InputText("Class Name", scriptBuffer, sizeof(scriptBuffer))) {
                        sc.ClassName = std::string(scriptBuffer);
                        TrackOverride(m_Context.SelectedEntity, "ScriptComponent.ClassName");
                    }
                    if (ImGui::Button("Remove Script Component")) {
                        m_Context.SelectedEntity.RemoveComponent<ScriptComponent>();
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
                ImGui::OpenPopup("AddComponentPopup");
            }
            
            if (ImGui::BeginPopup("AddComponentPopup")) {
                auto addComponentHelper = [&](auto componentType, const std::string& name) {
                    SceneSerializer serializer(*m_Context.ActiveScene);
                    nlohmann::json beforeState = serializer.SerializeToJson();
                    
                    m_Context.SelectedEntity.AddComponent<decltype(componentType)>();
                    
                    nlohmann::json afterState = serializer.SerializeToJson();
                    EditorHistory::PushCommand(
                        std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Add Component: " + name)
                    );
                    ImGui::CloseCurrentPopup();
                };

                if (!m_Context.SelectedEntity.HasComponent<TransformComponent>() && ImGui::MenuItem("Transform")) {
                    addComponentHelper(TransformComponent{}, "Transform");
                }
                if (!m_Context.SelectedEntity.HasComponent<SpriteRendererComponent>() && ImGui::MenuItem("Sprite Renderer")) {
                    addComponentHelper(SpriteRendererComponent{}, "Sprite Renderer");
                }
                if (!m_Context.SelectedEntity.HasComponent<MeshRendererComponent>() && ImGui::MenuItem("Mesh Renderer")) {
                    addComponentHelper(MeshRendererComponent{}, "Mesh Renderer");
                }
                if (!m_Context.SelectedEntity.HasComponent<VelocityComponent>() && ImGui::MenuItem("Velocity")) {
                    addComponentHelper(VelocityComponent{}, "Velocity");
                }
                if (!m_Context.SelectedEntity.HasComponent<HierarchyComponent>() && ImGui::MenuItem("Hierarchy")) {
                    addComponentHelper(HierarchyComponent{}, "Hierarchy");
                }
                if (!m_Context.SelectedEntity.HasComponent<SpriteAnimationComponent>() && ImGui::MenuItem("Sprite Animation")) {
                    addComponentHelper(SpriteAnimationComponent{}, "Sprite Animation");
                }
                if (!m_Context.SelectedEntity.HasComponent<TilemapComponent>() && ImGui::MenuItem("Tilemap Component")) {
                    addComponentHelper(TilemapComponent{}, "Tilemap");
                }
                if (!m_Context.SelectedEntity.HasComponent<AnimatorComponent>() && ImGui::MenuItem("Animator Component")) {
                    addComponentHelper(AnimatorComponent{}, "Animator");
                }
                if (!m_Context.SelectedEntity.HasComponent<AudioSourceComponent>() && ImGui::MenuItem("Audio Source Component")) {
                    addComponentHelper(AudioSourceComponent{}, "Audio Source");
                }
                if (!m_Context.SelectedEntity.HasComponent<ScriptComponent>() && ImGui::MenuItem("Script Component")) {
                    addComponentHelper(ScriptComponent{}, "Script");
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();
            if (ImGui::Button("Delete Entity", ImVec2(-1, 30))) {
                if (m_Context.DeleteEntityCallback) {
                    m_Context.DeleteEntityCallback(m_Context.SelectedEntity);
                } else {
                    SceneSerializer serializer(*m_Context.ActiveScene);
                    nlohmann::json beforeState = serializer.SerializeToJson();
                    
                    m_Context.ActiveScene->DestroyEntity(m_Context.SelectedEntity);
                    m_Context.SelectedEntity = {};
                    
                    nlohmann::json afterState = serializer.SerializeToJson();
                    EditorHistory::PushCommand(
                        std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Delete Entity")
                    );
                }
            }
        }
        ImGui::End();
    }

}
