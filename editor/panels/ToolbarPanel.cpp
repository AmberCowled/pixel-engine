#include "ToolbarPanel.hpp"
#include <imgui.h>
#include "editor/EditorIcons.hpp"
#include <engine/scripting/ScriptEngine.hpp>
#include <engine/ecs/Components.hpp>
#include <engine/base/Log.hpp>

namespace PixelEngine {

    void ToolbarPanel::OnImGuiRender() {
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            float buttonSize = 40.0f;
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x * 0.5f - (buttonSize * 2.0f));
            
            // Play Button
            bool isPlay = (m_Context.CurrentSceneState == SceneState::Play);
            if (ImGui::RadioButton(ICON_FA_PLAY " Play", isPlay)) {
                if (m_Context.CurrentSceneState == SceneState::Edit) {
                    m_Context.ActiveScene->StopAllAudio();
                    m_Context.CurrentSceneState = SceneState::Play;
                    m_Context.EditorScene = m_Context.ActiveScene; // save original active scene
                    m_Context.ActiveScene = Scene::Clone(m_Context.EditorScene);
                    m_Context.SelectedEntity = {};

                    ScriptEngine::SetActiveScene(m_Context.ActiveScene);
                    auto view = m_Context.ActiveScene->Reg().view<IDComponent>();
                    for (auto entityID : view) {
                        Entity entity = { entityID, m_Context.ActiveScene.get() };
                        if (entity.HasComponent<ScriptComponent>()) {
                            ScriptEngine::OnCreateEntity(entity);
                        }
                    }
                } else if (m_Context.CurrentSceneState == SceneState::Pause) {
                    m_Context.CurrentSceneState = SceneState::Play;
                }
            }
            ImGui::SameLine();
            
            // Pause Button
            bool isPause = (m_Context.CurrentSceneState == SceneState::Pause);
            if (ImGui::RadioButton(ICON_FA_PAUSE " Pause", isPause)) {
                if (m_Context.CurrentSceneState == SceneState::Play) {
                    m_Context.CurrentSceneState = SceneState::Pause;
                }
            }
            ImGui::SameLine();
            
            // Stop Button
            if (ImGui::Button(ICON_FA_STOP " Stop")) {
                if (m_Context.CurrentSceneState == SceneState::Play || m_Context.CurrentSceneState == SceneState::Pause) {
                    m_Context.ActiveScene->StopAllAudio();
                    m_Context.CurrentSceneState = SceneState::Edit;
                    m_Context.ActiveScene = m_Context.EditorScene; // restore edit scene
                    m_Context.SelectedEntity = {};

                    ScriptEngine::Reset();
                    ScriptEngine::SetActiveScene(m_Context.ActiveScene);
                }
            }

            // Snapping Settings popup/button on the right
            if (m_Context.ProjectLoaded) {
                ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 120.0f);
                if (ImGui::Button(ICON_FA_GEAR " Snapping")) {
                    ImGui::OpenPopup("SnapSettingsPopup");
                }
                
                if (ImGui::BeginPopup("SnapSettingsPopup")) {
                    ImGui::Text("Grid Snapping Settings");
                    ImGui::Separator();
                    
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragFloat("Translate", &m_Context.SnapTranslate, 0.05f, 0.01f, 10.0f, "%.2f");
                    
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragFloat("Rotate (Deg)", &m_Context.SnapRotate, 1.0f, 1.0f, 180.0f, "%.0f");
                    
                    ImGui::SetNextItemWidth(100.0f);
                    ImGui::DragFloat("Scale", &m_Context.SnapScale, 0.05f, 0.01f, 10.0f, "%.2f");
                    
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::End();
    }

}
