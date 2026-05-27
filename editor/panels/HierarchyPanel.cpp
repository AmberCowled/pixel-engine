#include "HierarchyPanel.hpp"
#include <imgui.h>
#include "editor/EditorIcons.hpp"
#include "editor/EditorContext.hpp"
#include "editor/EditorUtils.hpp"
#include "editor/EditorCommands.hpp"
#include <engine/ecs/SceneSerializer.hpp>
#include <engine/core/EditorHistory.hpp>
#include <engine/base/Log.hpp>
#include <algorithm>

namespace PixelEngine {

    void HierarchyPanel::OnImGuiRender() {
        ImGui::Begin("Hierarchy");
        m_Context.HierarchyFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        {
            // Search input box
            ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", m_Context.HierarchySearchBuffer, IM_ARRAYSIZE(m_Context.HierarchySearchBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                m_Context.HierarchySearchBuffer[0] = '\0';
            }

            // Filter chips
            auto filterChip = [&](const char* label, HierarchyFilter filterVal) {
                bool selected = (m_Context.CurrentHierarchyFilter == filterVal);
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.40f, 0.94f, 1.0f)); // indigo highlight
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.25f, 0.36f, 1.0f)); // deep slate grey
                }
                if (ImGui::Button(label)) {
                    m_Context.CurrentHierarchyFilter = filterVal;
                }
                ImGui::PopStyleColor();
            };

            filterChip("All", HierarchyFilter::All); ImGui::SameLine();
            filterChip("Script", HierarchyFilter::Script); ImGui::SameLine();
            filterChip("Sprite", HierarchyFilter::Sprite); ImGui::SameLine();
            filterChip("Audio", HierarchyFilter::Audio); ImGui::SameLine();
            filterChip("Tilemap", HierarchyFilter::Tilemap);
            ImGui::Separator();

            std::string searchQuery = m_Context.HierarchySearchBuffer;
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
            bool isFiltering = !searchQuery.empty() || m_Context.CurrentHierarchyFilter != HierarchyFilter::All;

            if (m_Context.ActiveScene) {
                auto idView = m_Context.ActiveScene->Reg().view<IDComponent>();
                for (auto ent : idView) {
                    Entity entity = { ent, m_Context.ActiveScene.get() };
                    
                    if (isFiltering) {
                        if (EntityMatchesFilters(entity, searchQuery)) {
                            // Draw flat list of nodes, forcing Leaf/NoTreePush flag
                            auto& tag = entity.GetComponent<TagComponent>().Tag;
                            auto myUUID = entity.GetComponent<IDComponent>().ID;
                            ImGuiTreeNodeFlags flags = ((m_Context.SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                            
                            std::string icon = "";
                            if (entity.HasComponent<AudioSourceComponent>()) icon = ICON_FA_VOLUME_HIGH " ";
                            else if (entity.HasComponent<TilemapComponent>()) icon = ICON_FA_MAP " ";
                            else if (entity.HasComponent<AnimatorComponent>()) icon = ICON_FA_FILM " ";
                            else if (entity.HasComponent<SpriteRendererComponent>()) icon = ICON_FA_IMAGE " ";
                            else if (entity.HasComponent<MeshRendererComponent>()) icon = ICON_FA_CUBES " ";
                            else icon = ICON_FA_GEAR " ";
      
                            std::string label = icon + tag;
                            UUID prefabID = 0;
                            bool isPrefab = IsPartOfPrefab(entity, m_Context.ActiveScene.get(), prefabID);
                            if (isPrefab) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.376f, 0.647f, 0.980f, 1.0f));
      
                            ImGui::TreeNodeEx((void*)(uint64_t)myUUID, flags, "%s", label.c_str());
                            if (isPrefab) ImGui::PopStyleColor();
      
                            if (ImGui::IsItemClicked()) {
                                m_Context.SelectedEntity = entity;
                            }
                            DrawEntityContextMenu(entity);
                        }
                    } else {
                        bool isRoot = true;
                        if (entity.HasComponent<HierarchyComponent>()) {
                            isRoot = (entity.GetComponent<HierarchyComponent>().Parent == 0);
                        }
                        if (isRoot) {
                            DrawEntityNode(entity);
                        }
                    }
                }
            }
 
            // Drop target on the panel background to clear parent
            ImGui::Dummy(ImGui::GetContentRegionAvail());
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID")) {
                    UUID draggedUUID = *(const UUID*)payload->Data;
                    auto draggedEntity = m_Context.ActiveScene->GetEntityByUUID(draggedUUID);
                    if (draggedEntity && draggedEntity.HasComponent<HierarchyComponent>()) {
                        auto& hc = draggedEntity.GetComponent<HierarchyComponent>();
                        if (hc.Parent != 0) {
                            auto oldParent = m_Context.ActiveScene->GetEntityByUUID(hc.Parent);
                            if (oldParent && oldParent.HasComponent<HierarchyComponent>()) {
                                auto& oldParentHc = oldParent.GetComponent<HierarchyComponent>();
                                oldParentHc.Children.erase(std::remove(oldParentHc.Children.begin(), oldParentHc.Children.end(), draggedUUID), oldParentHc.Children.end());
                            }
                            hc.Parent = 0;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }
 
            // Context menu for Hierarchy empty space
            if (ImGui::BeginPopupContextWindow("HierarchyWorkspaceContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::MenuItem("Create Empty", "Ctrl+Shift+N")) {
                    if (m_Context.CreatePresetEntityCallback) {
                        m_Context.CreatePresetEntityCallback("Empty Entity", 0);
                    }
                }
                if (ImGui::BeginMenu("Create Preset")) {
                    if (m_Context.CreatePresetEntityCallback) {
                        if (ImGui::MenuItem("Empty Entity")) m_Context.CreatePresetEntityCallback("Empty Entity", 0);
                        if (ImGui::MenuItem("Sprite Renderer")) m_Context.CreatePresetEntityCallback("Sprite Renderer", 0);
                        if (ImGui::MenuItem("Camera")) m_Context.CreatePresetEntityCallback("Camera", 0);
                        if (ImGui::MenuItem("Audio Source")) m_Context.CreatePresetEntityCallback("Audio Source", 0);
                        if (ImGui::MenuItem("Animator")) m_Context.CreatePresetEntityCallback("Animator", 0);
                        if (ImGui::MenuItem("Tilemap")) m_Context.CreatePresetEntityCallback("Tilemap", 0);
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();
            if (ImGui::Button("Add Entity")) {
                SceneSerializer serializer(*m_Context.ActiveScene);
                nlohmann::json beforeState = serializer.SerializeToJson();
                
                auto ent = m_Context.ActiveScene->CreateEntity("New Entity");
                m_Context.SelectedEntity = ent;
                
                nlohmann::json afterState = serializer.SerializeToJson();
                EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Add Entity")
                );
            }
        }
        ImGui::End();
    }

    void HierarchyPanel::DrawEntityNode(Entity entity) {
        auto& tag = entity.GetComponent<TagComponent>().Tag;
        auto myUUID = entity.GetComponent<IDComponent>().ID;
        
        ImGuiTreeNodeFlags flags = ((m_Context.SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        
        bool hasChildren = false;
        if (entity.HasComponent<HierarchyComponent>()) {
            hasChildren = !entity.GetComponent<HierarchyComponent>().Children.empty();
        }
        
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        
        std::string icon = "";
        if (entity.HasComponent<AudioSourceComponent>()) {
            icon = ICON_FA_VOLUME_HIGH " ";
        } else if (entity.HasComponent<TilemapComponent>()) {
            icon = ICON_FA_MAP " ";
        } else if (entity.HasComponent<AnimatorComponent>()) {
            icon = ICON_FA_FILM " ";
        } else if (entity.HasComponent<SpriteRendererComponent>()) {
            icon = ICON_FA_IMAGE " ";
        } else if (entity.HasComponent<MeshRendererComponent>()) {
            icon = ICON_FA_CUBES " ";
        } else {
            icon = ICON_FA_GEAR " ";
        }
        
        std::string label = icon + tag;
        
        UUID prefabID = 0;
        bool isPrefab = IsPartOfPrefab(entity, m_Context.ActiveScene.get(), prefabID);
        if (isPrefab) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.376f, 0.647f, 0.980f, 1.0f));
        }
        
        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)myUUID, flags, "%s", label.c_str());
        
        if (isPrefab) {
            ImGui::PopStyleColor();
        }
        
        if (ImGui::IsItemClicked()) {
            m_Context.SelectedEntity = entity;
        }
        
        DrawEntityContextMenu(entity);
        
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("ENTITY_UUID", &myUUID, sizeof(UUID));
            ImGui::Text("Dragging %s", tag.c_str());
            ImGui::EndDragDropSource();
        }
        
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID")) {
                UUID draggedUUID = *(const UUID*)payload->Data;
                if (draggedUUID != myUUID) {
                    auto draggedEntity = m_Context.ActiveScene->GetEntityByUUID(draggedUUID);
                    if (draggedEntity && !IsDescendantOf(draggedEntity, myUUID)) {
                        // Reparent
                        if (draggedEntity.HasComponent<HierarchyComponent>()) {
                            auto& draggedHc = draggedEntity.GetComponent<HierarchyComponent>();
                            if (draggedHc.Parent != 0) {
                                auto oldParent = m_Context.ActiveScene->GetEntityByUUID(draggedHc.Parent);
                                if (oldParent && oldParent.HasComponent<HierarchyComponent>()) {
                                    auto& oldParentHc = oldParent.GetComponent<HierarchyComponent>();
                                    oldParentHc.Children.erase(std::remove(oldParentHc.Children.begin(), oldParentHc.Children.end(), draggedUUID), oldParentHc.Children.end());
                                }
                            }
                        } else {
                            draggedEntity.AddComponent<HierarchyComponent>();
                        }
                        
                        auto& draggedHc = draggedEntity.GetComponent<HierarchyComponent>();
                        draggedHc.Parent = myUUID;
                        
                        if (!entity.HasComponent<HierarchyComponent>()) {
                            entity.AddComponent<HierarchyComponent>();
                        }
                        auto& ourHc = entity.GetComponent<HierarchyComponent>();
                        if (std::find(ourHc.Children.begin(), ourHc.Children.end(), draggedUUID) == ourHc.Children.end()) {
                            ourHc.Children.push_back(draggedUUID);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        if (opened && hasChildren) {
            auto& hc = entity.GetComponent<HierarchyComponent>();
            for (auto childUUID : hc.Children) {
                auto childEntity = m_Context.ActiveScene->GetEntityByUUID(childUUID);
                if (childEntity) {
                    DrawEntityNode(childEntity);
                }
            }
            ImGui::TreePop();
        }
    }

    void HierarchyPanel::DrawEntityContextMenu(Entity entity) {
        if (ImGui::BeginPopupContextItem()) {
            m_Context.SelectedEntity = entity;
            auto myUUID = entity.GetComponent<IDComponent>().ID;

            if (ImGui::MenuItem("Create Empty Child")) {
                if (m_Context.CreatePresetEntityCallback) {
                    m_Context.CreatePresetEntityCallback("Empty Entity", myUUID);
                }
            }
            if (ImGui::BeginMenu("Create Child Preset")) {
                if (m_Context.CreatePresetEntityCallback) {
                    if (ImGui::MenuItem("Empty Entity")) m_Context.CreatePresetEntityCallback("Empty Entity", myUUID);
                    if (ImGui::MenuItem("Sprite Renderer")) m_Context.CreatePresetEntityCallback("Sprite Renderer", myUUID);
                    if (ImGui::MenuItem("Camera")) m_Context.CreatePresetEntityCallback("Camera", myUUID);
                    if (ImGui::MenuItem("Audio Source")) m_Context.CreatePresetEntityCallback("Audio Source", myUUID);
                    if (ImGui::MenuItem("Animator")) m_Context.CreatePresetEntityCallback("Animator", myUUID);
                    if (ImGui::MenuItem("Tilemap")) m_Context.CreatePresetEntityCallback("Tilemap", myUUID);
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Group Selected")) {
                if (m_Context.GroupEntityCallback) {
                    m_Context.GroupEntityCallback(entity);
                }
            }
            if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
                if (m_Context.DuplicateSubtreeCallback) {
                    m_Context.SelectedEntity = m_Context.DuplicateSubtreeCallback(entity, 0, " (Copy)");
                }
            }
            if (ImGui::MenuItem("Rename", "F2")) {
                m_Context.TriggerRenamePopup = true;
            }
            if (ImGui::MenuItem("Delete", "Delete")) {
                SceneSerializer serializer(*m_Context.ActiveScene);
                nlohmann::json beforeState = serializer.SerializeToJson();
                
                if (m_Context.DeleteEntityCallback) {
                    m_Context.DeleteEntityCallback(entity);
                }
                if (m_Context.SelectedEntity == entity) m_Context.SelectedEntity = {};

                nlohmann::json afterState = serializer.SerializeToJson();
                EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Delete Entity")
                );
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Convert to Prefab")) {
                std::filesystem::path folderPath = m_Context.ProjectLoaded ? (std::filesystem::path(m_Context.ProjectPath) / "assets") : std::filesystem::path("assets");
                if (m_Context.ProjectLoaded && !m_Context.CurrentDirectory.empty()) {
                    folderPath = m_Context.CurrentDirectory;
                }
                if (m_Context.SaveEntityAsPrefabCallback) {
                    m_Context.SaveEntityAsPrefabCallback(myUUID, folderPath);
                }
            }
            ImGui::Separator();
            
            bool hasParent = false;
            if (entity.HasComponent<HierarchyComponent>()) {
                hasParent = (entity.GetComponent<HierarchyComponent>().Parent != 0);
            }
            if (ImGui::MenuItem("Move Up", nullptr, false, hasParent)) {
                SceneSerializer serializer(*m_Context.ActiveScene);
                nlohmann::json beforeState = serializer.SerializeToJson();

                if (m_Context.ReorderEntityCallback) {
                    m_Context.ReorderEntityCallback(entity, true);
                }

                nlohmann::json afterState = serializer.SerializeToJson();
                EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Move Entity Up")
                );
            }
            if (ImGui::MenuItem("Move Down", nullptr, false, hasParent)) {
                SceneSerializer serializer(*m_Context.ActiveScene);
                nlohmann::json beforeState = serializer.SerializeToJson();

                if (m_Context.ReorderEntityCallback) {
                    m_Context.ReorderEntityCallback(entity, false);
                }

                nlohmann::json afterState = serializer.SerializeToJson();
                EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Move Entity Down")
                );
            }
            ImGui::EndPopup();
        }
    }

    bool HierarchyPanel::EntityMatchesFilters(Entity entity, const std::string& searchQuery) {
        if (!searchQuery.empty()) {
            std::string tag = entity.GetComponent<TagComponent>().Tag;
            std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
            if (tag.find(searchQuery) == std::string::npos) {
                return false;
            }
        }

        switch (m_Context.CurrentHierarchyFilter) {
            case HierarchyFilter::Script: return entity.HasComponent<ScriptComponent>();
            case HierarchyFilter::Sprite: return entity.HasComponent<SpriteRendererComponent>();
            case HierarchyFilter::Audio: return entity.HasComponent<AudioSourceComponent>();
            case HierarchyFilter::Tilemap: return entity.HasComponent<TilemapComponent>();
            default: return true;
        }
    }

    bool HierarchyPanel::IsDescendantOf(Entity entity, UUID potentialParentUUID) {
        if (!entity || !entity.HasComponent<HierarchyComponent>()) return false;

        auto& hc = entity.GetComponent<HierarchyComponent>();
        for (auto childUUID : hc.Children) {
            if (childUUID == potentialParentUUID) return true;
            auto childEntity = m_Context.ActiveScene->GetEntityByUUID(childUUID);
            if (childEntity && IsDescendantOf(childEntity, potentialParentUUID)) {
                return true;
            }
        }
        return false;
    }

}
