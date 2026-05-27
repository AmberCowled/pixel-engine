#include "TilemapPaintingPanel.hpp"
#include <imgui.h>
#include "editor/EditorIcons.hpp"
#include <engine/assets/AssetManager.hpp>
#include <engine/ecs/Components.hpp>
#include <fstream>
#include <iomanip>

namespace PixelEngine {

    void TilemapPaintingPanel::OnImGuiRender() {
        ImGui::Begin("Tilemap Painting");
        if (m_Context.SelectedEntity && m_Context.SelectedEntity.HasComponent<TilemapComponent>()) {
            auto& tc = m_Context.SelectedEntity.GetComponent<TilemapComponent>();
            
            // List tileset assets in project
            ImGui::Text("Available Tilesets:");
            for (const auto& [uuid, meta] : AssetManager::GetMetadataRegistry()) {
                if (meta.Type == AssetType::Tileset) {
                    bool isSelected = (tc.TilesetID == uuid);
                    if (ImGui::Selectable(meta.SourcePath.c_str(), isSelected)) {
                        tc.TilesetID = uuid;
                    }
                }
            }

            ImGui::Separator();
            auto tileset = AssetManager::GetTileset(tc.TilesetID);
            
            if (tileset) {
                // Brush selection
                bool isPaint = (m_Context.CurrentBrushType == BrushType::Paint);
                bool isErase = (m_Context.CurrentBrushType == BrushType::Erase);
                
                if (ImGui::RadioButton(ICON_FA_PAINT_BRUSH " Paint Brush", isPaint)) {
                    m_Context.CurrentBrushType = BrushType::Paint;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton(ICON_FA_ERASER " Eraser", isErase)) {
                    m_Context.CurrentBrushType = BrushType::Erase;
                }
                
                ImGui::Separator();
                ImGui::Text("Palette Grid");
                
                auto texture = AssetManager::GetTexture(tileset->TextureID);
                if (texture) {
                    VkDescriptorSet textureDS = AssetManager::GetTextureDescriptorSet(tileset->TextureID);
                    if (textureDS != VK_NULL_HANDLE) {
                        float panelWidth = ImGui::GetContentRegionAvail().x;
                        float scale = panelWidth / texture->GetWidth();
                        ImVec2 displaySize(panelWidth, texture->GetHeight() * scale);
                        
                        ImVec2 startPos = ImGui::GetCursorScreenPos();
                        ImGui::Image((ImTextureID)textureDS, displaySize);
                        
                        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                            ImVec2 mousePos = ImGui::GetMousePos();
                            float localX = mousePos.x - startPos.x;
                            float localY = mousePos.y - startPos.y;
                            
                            float texX = localX / scale;
                            float texY = localY / scale;
                            
                            uint32_t col = static_cast<uint32_t>(texX) / tileset->TileSize;
                            uint32_t row = static_cast<uint32_t>(texY) / tileset->TileSize;
                            uint32_t cols = texture->GetWidth() / tileset->TileSize;
                            
                            m_Context.SelectedTileIndex = row * cols + col + 1; // 1-based index
                        }
                        
                        // Draw highlight on selected tile
                        if (m_Context.SelectedTileIndex > 0) {
                            uint32_t cols = texture->GetWidth() / tileset->TileSize;
                            uint32_t col = (m_Context.SelectedTileIndex - 1) % cols;
                            uint32_t row = (m_Context.SelectedTileIndex - 1) / cols;
                            
                            ImVec2 tileStart(startPos.x + col * tileset->TileSize * scale, startPos.y + row * tileset->TileSize * scale);
                            ImVec2 tileEnd(tileStart.x + tileset->TileSize * scale, tileStart.y + tileset->TileSize * scale);
                            
                            ImGui::GetWindowDrawList()->AddRect(tileStart, tileEnd, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
                        }
                    }
                } else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Tileset texture not loaded.");
                }
                
                ImGui::Separator();
                if (m_Context.SelectedTileIndex > 0) {
                    ImGui::Text("Selected Tile Index: %u", m_Context.SelectedTileIndex);
                    bool isSolid = tileset->SolidTiles[m_Context.SelectedTileIndex];
                    if (ImGui::Checkbox("Is Solid Tile (Collision)", &isSolid)) {
                        tileset->SolidTiles[m_Context.SelectedTileIndex] = isSolid;
                        // Save changes to disk
                        std::string tilesetPath = AssetManager::GetAssetPath(tileset->ID);
                        std::ofstream fout(tilesetPath);
                        if (fout.is_open()) {
                            nlohmann::json tsJson;
                            tsJson["tileSize"] = tileset->TileSize;
                            std::string texRelPath = "";
                            auto texMetadata = AssetManager::GetMetadataRegistry().find(tileset->TextureID);
                            if (texMetadata != AssetManager::GetMetadataRegistry().end()) {
                                texRelPath = texMetadata->second.SourcePath;
                            }
                            tsJson["texturePath"] = texRelPath;
                            nlohmann::json solidArray = nlohmann::json::array();
                            for (const auto& [tileIdx, solid] : tileset->SolidTiles) {
                                if (solid) {
                                    solidArray.push_back(tileIdx);
                                }
                            }
                            tsJson["solidTiles"] = solidArray;
                            fout << std::setw(4) << tsJson << std::endl;
                        }
                    }
                } else {
                    ImGui::Text("No tile selected");
                }
            } else {
                ImGui::Text("No tileset loaded on this component.");
            }
        } else {
            ImGui::Text("Select an entity with a Tilemap Component to paint.");
        }
        ImGui::End();
    }

}
