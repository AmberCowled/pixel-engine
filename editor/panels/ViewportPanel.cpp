#include "ViewportPanel.hpp"
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>
#include <SDL3/SDL.h>
#include "editor/EditorIcons.hpp"
#include "editor/EditorContext.hpp"
#include "editor/EditorUtils.hpp"
#include "editor/EditorCommands.hpp"
#include <engine/renderer/VulkanContext.hpp>
#include <engine/ecs/Components.hpp>
#include <engine/ecs/SceneSerializer.hpp>
#include <engine/core/EditorHistory.hpp>
#include <engine/base/Log.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace PixelEngine {

    void ViewportPanel::OnImGuiRender() {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGui::Begin("Viewport");
        {
            m_Context.ViewportFocused = ImGui::IsWindowFocused();
            m_Context.ViewportHovered = ImGui::IsWindowHovered();

            ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
            
            if (m_Context.VulkanCtx && (viewportPanelSize.x != m_Context.ViewportSize.x || viewportPanelSize.y != m_Context.ViewportSize.y)) {
                m_Context.ViewportSize = glm::vec2(viewportPanelSize.x, viewportPanelSize.y);
                
                float dpiScale = 1.0f;
                if (m_Context.Window) {
                    dpiScale = SDL_GetWindowDisplayScale(m_Context.Window);
                }
                uint32_t width = std::max((uint32_t)(m_Context.ViewportSize.x * dpiScale), 64u);
                uint32_t height = std::max((uint32_t)(m_Context.ViewportSize.y * dpiScale), 64u);

                vkDeviceWaitIdle(m_Context.VulkanCtx->GetDevice());
                
                OffscreenTargetConfig offscreenConfig{};
                offscreenConfig.width = width;
                offscreenConfig.height = height;
                offscreenConfig.colorFormat = VK_FORMAT_R8G8B8A8_SRGB;
                offscreenConfig.depthFormat = m_Context.VulkanCtx->FindSupportedFormat(
                    {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                );

                m_Context.OffscreenBuffer = std::make_unique<OffscreenTarget>(
                    *m_Context.VulkanCtx, offscreenConfig, m_Context.VulkanCtx->GetOffscreenRenderPass()
                );

                m_Context.VulkanCtx->UpdateUpscaleDescriptorSets(m_Context.OffscreenBuffer->GetColorImageView());

                // Recreate ImGui viewport descriptor set
                if (m_Context.ViewportDescriptorSet != VK_NULL_HANDLE) {
                    ImGui_ImplVulkan_RemoveTexture(m_Context.ViewportDescriptorSet);
                }
                m_Context.ViewportDescriptorSet = ImGui_ImplVulkan_AddTexture(
                    m_Context.VulkanCtx->GetTextureSampler(),
                    m_Context.OffscreenBuffer->GetColorImageView(),
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
            }

            if (m_Context.ViewportDescriptorSet != VK_NULL_HANDLE) {
                ImGui::Image((ImTextureID)m_Context.ViewportDescriptorSet, ImVec2{ m_Context.ViewportSize.x, m_Context.ViewportSize.y });
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    const char* assetPath = (const char*)payload->Data;
                    std::string pathStr(assetPath);
                    if (pathStr.ends_with(".prefab.json") || pathStr.ends_with(".prefab")) {
                        if (m_Context.InstantiatePrefabCallback) {
                            m_Context.InstantiatePrefabCallback(pathStr);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImVec2 imageMin = ImGui::GetItemRectMin();
            ImVec2 imageSize = ImGui::GetItemRectSize();

            if (m_Context.SelectedEntity && m_Context.SelectedEntity.HasComponent<TilemapComponent>() && m_Context.CurrentSceneState == SceneState::Edit) {
                if (m_Context.ViewportHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        SceneSerializer serializer(*m_Context.ActiveScene);
                        m_Context.BeforePaintState = serializer.SerializeToJson();
                        m_Context.IsPainting = true;
                    }

                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_Context.IsPainting) {
                        ImVec2 mousePos = ImGui::GetMousePos();
                        float mx = mousePos.x - imageMin.x;
                        float my = mousePos.y - imageMin.y;
                        
                        if (mx >= 0.0f && mx < imageSize.x && my >= 0.0f && my < imageSize.y) {
                            float ndcX = (mx / imageSize.x) * 2.0f - 1.0f;
                            float ndcY = 1.0f - (my / imageSize.y) * 2.0f;
                            
                            glm::mat4 projection = m_Context.EditorCamera.GetProjection();
                            glm::mat4 view = m_Context.EditorCamera.GetView();
                            glm::mat4 invVP = glm::inverse(projection * view);
                            
                            glm::vec4 nearPt = invVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
                            glm::vec4 farPt = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
                            
                            nearPt /= nearPt.w;
                            farPt /= farPt.w;
                            
                            glm::vec3 rayOrigin = glm::vec3(nearPt);
                            glm::vec3 rayDir = glm::normalize(glm::vec3(farPt - nearPt));
                            
                            if (glm::abs(rayDir.z) > 0.0001f) {
                                float t = -rayOrigin.z / rayDir.z;
                                if (t >= 0.0f) {
                                    glm::vec3 intersection = rayOrigin + t * rayDir;
                                    
                                    glm::mat4 invWorldTransform = glm::inverse(m_Context.ActiveScene->GetWorldTransform(m_Context.SelectedEntity));
                                    glm::vec4 localIntersection = invWorldTransform * glm::vec4(intersection, 1.0f);
                                    
                                    int tileX = static_cast<int>(std::floor(localIntersection.x));
                                    int tileY = static_cast<int>(std::floor(localIntersection.y));
                                    
                                    int chunkX = tileX >= 0 ? tileX / 16 : (tileX - 15) / 16;
                                    int chunkY = tileY >= 0 ? tileY / 16 : (tileY - 15) / 16;
                                    int localX = tileX - chunkX * 16;
                                    int localY = tileY - chunkY * 16;
                                    
                                    auto& tc = m_Context.SelectedEntity.GetComponent<TilemapComponent>();
                                    auto chunkCoords = std::make_pair(chunkX, chunkY);
                                    
                                    if (m_Context.CurrentBrushType == BrushType::Paint) {
                                        tc.Chunks[chunkCoords].Tiles[localY * 16 + localX].TileIndex = m_Context.SelectedTileIndex;
                                        TrackOverride(m_Context.SelectedEntity, "TilemapComponent.Chunks");
                                    } else if (m_Context.CurrentBrushType == BrushType::Erase) {
                                        tc.Chunks[chunkCoords].Tiles[localY * 16 + localX].TileIndex = 0;
                                        TrackOverride(m_Context.SelectedEntity, "TilemapComponent.Chunks");
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (m_Context.IsPainting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                SceneSerializer serializer(*m_Context.ActiveScene);
                nlohmann::json afterPaintState = serializer.SerializeToJson();
                EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, m_Context.BeforePaintState, afterPaintState, "Paint Tilemap")
                );
                m_Context.IsPainting = false;
            }

            // Editor 2D Viewport Camera Controls (Pan & Zoom centered on cursor)
            ImGuiIO& io = ImGui::GetIO();
            if (m_Context.ViewportHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
                // Determine mouse pos under cursor for zoom centering
                ImVec2 mousePos = ImGui::GetMousePos();
                float mx = mousePos.x - imageMin.x;
                float my = mousePos.y - imageMin.y;

                glm::vec3 worldMouseBefore(0.0f);
                bool hasMousePos = false;

                if (mx >= 0.0f && mx < imageSize.x && my >= 0.0f && my < imageSize.y) {
                    float ndcX = (mx / imageSize.x) * 2.0f - 1.0f;
                    float ndcY = 1.0f - (my / imageSize.y) * 2.0f;

                    glm::mat4 projection = m_Context.EditorCamera.GetProjection();
                    glm::mat4 view = m_Context.EditorCamera.GetView();
                    glm::mat4 invVP = glm::inverse(projection * view);

                    glm::vec4 nearPt = invVP * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
                    glm::vec4 farPt = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);

                    nearPt /= nearPt.w;
                    farPt /= farPt.w;

                    glm::vec3 rayOrigin = glm::vec3(nearPt);
                    glm::vec3 rayDir = glm::normalize(glm::vec3(farPt - nearPt));

                    if (glm::abs(rayDir.z) > 0.0001f) {
                        float t = -rayOrigin.z / rayDir.z;
                        if (t >= 0.0f) {
                            worldMouseBefore = rayOrigin + t * rayDir;
                            hasMousePos = true;
                        }
                    }
                }

                // Handle Zoom
                if (io.MouseWheel != 0.0f) {
                    float oldDistance = m_Context.CameraDistance;
                    m_Context.CameraDistance -= io.MouseWheel * 0.25f * (m_Context.CameraDistance * 0.1f);
                    m_Context.CameraDistance = glm::clamp(m_Context.CameraDistance, 0.5f, 100.0f);

                    // Adjust camera target to keep mouse cursor over the same world position
                    if (hasMousePos) {
                        m_Context.CameraTarget.x = worldMouseBefore.x - (m_Context.CameraDistance / oldDistance) * (worldMouseBefore.x - m_Context.CameraTarget.x);
                        m_Context.CameraTarget.y = worldMouseBefore.y - (m_Context.CameraDistance / oldDistance) * (worldMouseBefore.y - m_Context.CameraTarget.y);
                    }
                }

                // Handle Pan
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                    ImVec2 mouseDelta = io.MouseDelta;
                    float aspect = imageSize.x / imageSize.y;
                    float tanHalfFovy = tan(glm::radians(45.0f) / 2.0f);
                    float worldUnitsPerPixelY = 2.0f * m_Context.CameraDistance * tanHalfFovy / imageSize.y;
                    float worldUnitsPerPixelX = worldUnitsPerPixelY * aspect;

                    m_Context.CameraTarget.x += mouseDelta.x * worldUnitsPerPixelX;
                    m_Context.CameraTarget.y += mouseDelta.y * worldUnitsPerPixelY;
                }
            }

            // Keyboard shortcut for Focus selection (F key)
            if (m_Context.SelectedEntity && m_Context.SelectedEntity.HasComponent<TransformComponent>() && ImGui::IsKeyPressed(ImGuiKey_F) && !io.WantTextInput) {
                auto& tc = m_Context.SelectedEntity.GetComponent<TransformComponent>();
                m_Context.CameraTarget.x = tc.Translation.x;
                m_Context.CameraTarget.y = tc.Translation.y;
            }

            // Lock pitch and yaw to view straight down Z axis
            glm::vec3 cameraPos = glm::vec3(m_Context.CameraTarget.x, m_Context.CameraTarget.y, m_Context.CameraDistance);
            m_Context.EditorCamera.SetViewTarget(cameraPos, glm::vec3(m_Context.CameraTarget.x, m_Context.CameraTarget.y, 0.0f));

            // ImGuizmo Manipulator
            if (m_Context.SelectedEntity && m_Context.GizmoType >= 0 && m_Context.CurrentSceneState == SceneState::Edit) {
                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist();
                
                ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
                
                glm::mat4 cameraProjection = m_Context.EditorCamera.GetProjection();
                glm::mat4 cameraView = m_Context.EditorCamera.GetView();
                
                auto& tc = m_Context.SelectedEntity.GetComponent<TransformComponent>();
                glm::mat4 transform = tc.GetTransform();
                
                bool snap = io.KeyCtrl;
                float snapValue = 0.5f;
                if (m_Context.GizmoType == ImGuizmo::TRANSLATE) {
                    snapValue = 0.5f;
                } else if (m_Context.GizmoType == ImGuizmo::ROTATE) {
                    snapValue = 45.0f;
                }
                float snapValues[3] = { snapValue, snapValue, snapValue };
                
                ImGuizmo::Manipulate(
                    glm::value_ptr(cameraView), 
                    glm::value_ptr(cameraProjection), 
                    (ImGuizmo::OPERATION)m_Context.GizmoType, 
                    ImGuizmo::LOCAL, 
                    glm::value_ptr(transform), 
                    nullptr, 
                    snap ? snapValues : nullptr
                );
                
                static nlohmann::json beforeGizmoState;
                static bool wasGizmoUsing = false;

                if (ImGuizmo::IsUsing()) {
                    if (!wasGizmoUsing) {
                        SceneSerializer serializer(*m_Context.ActiveScene);
                        beforeGizmoState = serializer.SerializeToJson();
                        wasGizmoUsing = true;
                    }
                    glm::vec3 translation, rotation, scale;
                    ImGuizmo::DecomposeMatrixToComponents(
                        glm::value_ptr(transform), 
                        glm::value_ptr(translation), 
                        glm::value_ptr(rotation), 
                        glm::value_ptr(scale)
                    );
                    
                    glm::vec3 deltaRotation = rotation - tc.Rotation;
                    tc.Translation = translation;
                    tc.Rotation += deltaRotation;
                    tc.Scale = scale;
                } else {
                    if (wasGizmoUsing) {
                        SceneSerializer serializer(*m_Context.ActiveScene);
                        nlohmann::json afterGizmoState = serializer.SerializeToJson();
                        EditorHistory::PushCommand(
                            std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeGizmoState, afterGizmoState, "Gizmo Transform")
                        );
                        wasGizmoUsing = false;
                    }
                }
            }

            // Context menu for Viewport
            if (ImGui::BeginPopupContextWindow("ViewportContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::BeginMenu("Create Entity")) {
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
                if (m_Context.SelectedEntity) {
                    ImGui::Separator();
                    if (ImGui::MenuItem("Duplicate Entity", "Ctrl+D")) {
                        if (m_Context.DuplicateSubtreeCallback) {
                            m_Context.SelectedEntity = m_Context.DuplicateSubtreeCallback(m_Context.SelectedEntity, 0, " (Copy)");
                        }
                    }
                    if (ImGui::MenuItem("Delete Entity", "Delete")) {
                        SceneSerializer serializer(*m_Context.ActiveScene);
                        nlohmann::json beforeState = serializer.SerializeToJson();
                        
                        if (m_Context.DeleteEntityCallback) {
                            m_Context.DeleteEntityCallback(m_Context.SelectedEntity);
                        }
                        m_Context.SelectedEntity = {};

                        nlohmann::json afterState = serializer.SerializeToJson();
                        EditorHistory::PushCommand(
                            std::make_unique<SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Delete Entity")
                        );
                    }
                    if (m_Context.SelectedEntity.HasComponent<TransformComponent>()) {
                        if (ImGui::MenuItem("Focus Camera", "F")) {
                            auto& tc = m_Context.SelectedEntity.GetComponent<TransformComponent>();
                            m_Context.CameraTarget.x = tc.Translation.x;
                            m_Context.CameraTarget.y = tc.Translation.y;
                        }
                    }
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();
    }

}
