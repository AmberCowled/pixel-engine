#include "AssetBrowserPanel.hpp"
#include <imgui.h>
#include "editor/EditorIcons.hpp"
#include "editor/EditorContext.hpp"
#include <engine/assets/AssetManager.hpp>
#include <engine/ecs/Components.hpp>
#include <engine/base/Log.hpp>
#include <algorithm>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif

namespace PixelEngine {

    static bool IsHiddenPath(const std::filesystem::path& path) {
        std::string filename = path.filename().string();
        std::string ext = path.extension().string();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Hide files ending with .meta
        if (filename.find(".meta") != std::string::npos || ext == ".meta") {
            return true;
        }

        // Hide specific internal directories
        if (filename == ".git" || filename == "build" || filename == "bin" || 
            filename == "obj" || filename == "lib" || filename == ".vs" || 
            filename == ".idea" || filename == ".vscode" || filename == "scratch") {
            return true;
        }

        // Hide specific internal files
        if (ext == ".csproj" || ext == ".sln" || ext == ".suo" || ext == ".user" ||
            ext == ".dll" || ext == ".exe" || ext == ".pdb" || ext == ".log" || 
            filename == "imgui.ini" || filename == "asset_registry.json") {
            return true;
        }

        return false;
    }

    void AssetBrowserPanel::OnImGuiRender() {
        ImGui::Begin("Asset Browser");
        m_Context.AssetBrowserFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        {
            std::filesystem::path assetRoot = m_Context.ProjectLoaded ? std::filesystem::path(m_Context.ProjectPath) : std::filesystem::path("assets");
            if (m_Context.CurrentDirectory.empty() || !std::filesystem::exists(m_Context.CurrentDirectory)) {
                m_Context.CurrentDirectory = assetRoot;
            }

            // Search input box
            ImGui::InputTextWithHint("##AssetSearch", "Search assets...", m_Context.AssetSearchBuffer, IM_ARRAYSIZE(m_Context.AssetSearchBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                m_Context.AssetSearchBuffer[0] = '\0';
            }

            // Filter chips
            auto assetFilterChip = [&](const char* label, AssetFilter filterVal) {
                bool selected = (m_Context.CurrentAssetFilter == filterVal);
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.40f, 0.94f, 1.0f)); // indigo highlight
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.25f, 0.36f, 1.0f)); // deep slate grey
                }
                if (ImGui::Button(label)) {
                    m_Context.CurrentAssetFilter = filterVal;
                }
                ImGui::PopStyleColor();
            };

            assetFilterChip("All", AssetFilter::All); ImGui::SameLine();
            assetFilterChip("Textures", AssetFilter::Textures); ImGui::SameLine();
            assetFilterChip("Audio", AssetFilter::Audio); ImGui::SameLine();
            assetFilterChip("Scripts", AssetFilter::Scripts); ImGui::SameLine();
            assetFilterChip("Prefabs", AssetFilter::Prefabs); ImGui::SameLine();
            assetFilterChip("Scenes", AssetFilter::Scenes);
            ImGui::Separator();

            std::string searchQuery = m_Context.AssetSearchBuffer;
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
            bool isSearching = !searchQuery.empty() || m_Context.CurrentAssetFilter != AssetFilter::All;

            if (!isSearching) {
                // Draw navigation breadcrumbs
                if (m_Context.CurrentDirectory != assetRoot) {
                    if (ImGui::Button("<- Back")) {
                        m_Context.CurrentDirectory = m_Context.CurrentDirectory.parent_path();
                    }
                    ImGui::SameLine();
                    ImGui::Text("Current Path: %s", std::filesystem::relative(m_Context.CurrentDirectory, assetRoot).string().c_str());
                } else {
                    ImGui::Text(m_Context.ProjectLoaded ? "Project Root" : "Assets Root");
                }
                ImGui::Separator();
            } else {
                ImGui::Text("Search Results (Flat Grid)");
                ImGui::Separator();
            }

            // Draw Grid table
            float cellSize = 96.0f + 16.0f;
            float panelWidth = ImGui::GetContentRegionAvail().x;
            int columns = (int)(panelWidth / cellSize);
            if (columns < 1) columns = 1;

            if (ImGui::BeginTable("AssetBrowserGridTable", columns)) {
                if (isSearching) {
                    for (auto it = std::filesystem::recursive_directory_iterator(assetRoot, std::filesystem::directory_options::skip_permission_denied);
                         it != std::filesystem::recursive_directory_iterator(); ++it) {
                        if (IsHiddenPath(it->path())) {
                            it.disable_recursion_pending();
                            continue;
                        }
                        if (it->is_regular_file()) {
                            auto path = it->path();
                            if (MatchAssetFilter(path) && MatchAssetSearch(path, searchQuery)) {
                                ImGui::TableNextColumn();
                                DrawAssetGridItem(path, assetRoot);
                            }
                        }
                    }
                } else {
                    // 1. Draw directories
                    for (const auto& entry : std::filesystem::directory_iterator(m_Context.CurrentDirectory, std::filesystem::directory_options::skip_permission_denied)) {
                        if (IsHiddenPath(entry.path())) continue;
                        if (entry.is_directory()) {
                            ImGui::TableNextColumn();
                            DrawDirectoryGridItem(entry.path(), assetRoot);
                        }
                    }
                    // 2. Draw files
                    for (const auto& entry : std::filesystem::directory_iterator(m_Context.CurrentDirectory, std::filesystem::directory_options::skip_permission_denied)) {
                        if (IsHiddenPath(entry.path())) continue;
                        if (entry.is_regular_file()) {
                            auto path = entry.path();
                            if (MatchAssetFilter(path)) {
                                ImGui::TableNextColumn();
                                DrawAssetGridItem(path, assetRoot);
                            }
                        }
                    }
                }
                ImGui::EndTable();
            }

            // Context menu for empty space in Asset Browser
            if (ImGui::BeginPopupContextWindow("AssetBrowserEmptyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
                if (ImGui::BeginMenu("Create")) {
                    if (ImGui::MenuItem("New Folder")) {
                        m_Context.TriggerCreateFolderPopup = true;
                    }
                    if (ImGui::MenuItem("C# Script")) {
                        m_Context.TriggerCreateScriptPopup = true;
                    }
                    if (ImGui::MenuItem("Metadata File")) {
                        m_Context.TriggerCreateMetadataPopup = true;
                    }
                    ImGui::EndMenu();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Show in Explorer")) {
                    #ifdef _WIN32
                    std::string pathStr = m_Context.CurrentDirectory.string();
                    std::replace(pathStr.begin(), pathStr.end(), '/', '\\');
                    ShellExecuteA(NULL, "open", pathStr.c_str(), NULL, NULL, SW_SHOWNORMAL);
                    #endif
                }
                ImGui::EndPopup();
            }
        }
        ImGui::End();
    }

    void AssetBrowserPanel::DrawDirectoryGridItem(const std::filesystem::path& path, const std::filesystem::path& assetRoot) {
        auto filename = path.filename().string();
        ImGui::BeginGroup();
        
        bool isSelected = (m_Context.SelectedAssetPath == path);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.40f, 0.94f, 1.0f)); // indigo highlight
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.2f, 0.3f, 1.0f));
        }
        if (ImGui::Button((std::string(ICON_FA_FOLDER "\n\n") + filename).c_str(), ImVec2(80, 80))) {
            m_Context.SelectedAssetPath = path;
        }
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            m_Context.CurrentDirectory = path;
            m_Context.SelectedAssetPath = "";
        }

        if (ImGui::BeginPopupContextItem()) {
            m_Context.SelectedAssetPath = path;
            if (ImGui::MenuItem("Rename", "F2")) {
                m_Context.TriggerAssetRenamePopup = true;
            }
            if (ImGui::MenuItem("Delete", "Delete")) {
                std::error_code ec;
                std::filesystem::remove_all(path, ec);
                if (ec) {
                    PX_CORE_ERROR("Failed to delete directory: {0}", ec.message());
                }
                m_Context.SelectedAssetPath = "";
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Show in Explorer")) {
                #ifdef _WIN32
                std::string args = "/select,\"" + path.string() + "\"";
                std::replace(args.begin(), args.end(), '/', '\\');
                ShellExecuteA(NULL, "open", "explorer.exe", args.c_str(), NULL, SW_SHOWNORMAL);
                #endif
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID")) {
                UUID entityUUID = *(const UUID*)payload->Data;
                if (m_Context.SaveEntityAsPrefabCallback) {
                    m_Context.SaveEntityAsPrefabCallback(entityUUID, path);
                }
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 80.0f);
        ImGui::Text("%s", filename.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
    }

    void AssetBrowserPanel::DrawAssetGridItem(const std::filesystem::path& path, const std::filesystem::path& assetRoot) {
        auto filename = path.filename().string();
        std::string ext = path.extension().string();
        ImGui::BeginGroup();

        bool isSelected = (m_Context.SelectedAssetPath == path);
        bool drawn = false;
        if (ext == ".png" || ext == ".jpg" || ext == ".tga") {
            std::string relativePath = std::filesystem::relative(path, assetRoot).string();
            UUID uuid = AssetManager::LoadTexture(relativePath);
            if (uuid != 0) {
                VkDescriptorSet descriptorSet = AssetManager::GetTextureDescriptorSet(uuid);
                if (descriptorSet != VK_NULL_HANDLE) {
                    ImGui::Image((ImTextureID)descriptorSet, ImVec2(80, 80));
                    drawn = true;
                    if (ImGui::IsItemClicked()) {
                        m_Context.SelectedAssetPath = path;
                    }
                    if (isSelected) {
                        ImDrawList* drawList = ImGui::GetWindowDrawList();
                        ImVec2 min = ImGui::GetItemRectMin();
                        ImVec2 max = ImGui::GetItemRectMax();
                        drawList->AddRect(min, max, IM_COL32(99, 102, 241, 255), 4.0f, 0, 3.0f);
                    }
                }
            }
        }

        if (!drawn) {
            std::string iconLabel = ICON_FA_FILE "\n\nFile";
            if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") {
                iconLabel = ICON_FA_VOLUME_HIGH "\n\nAudio";
            } else if (ext == ".prefab" || filename.ends_with(".prefab.json")) {
                iconLabel = ICON_FA_CUBES "\n\nPrefab";
            } else if (ext == ".json") {
                iconLabel = ICON_FA_MAP "\n\nScene";
            } else if (ext == ".cs") {
                iconLabel = ICON_FA_FILE "\n\nScript";
            }
            
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.40f, 0.94f, 1.0f)); // indigo highlight
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.2f, 0.3f, 1.0f));
            }
            if (ImGui::Button((iconLabel + "\n" + filename).c_str(), ImVec2(80, 80))) {
                m_Context.SelectedAssetPath = path;
            }
            ImGui::PopStyleColor();
        }

        if (ImGui::BeginPopupContextItem()) {
            m_Context.SelectedAssetPath = path;
            bool isPrefab = (ext == ".prefab" || filename.ends_with(".prefab.json"));
            if (ImGui::MenuItem("Instantiate", nullptr, false, isPrefab)) {
                if (m_Context.InstantiatePrefabCallback) {
                    m_Context.InstantiatePrefabCallback(path.string());
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Rename", "F2")) {
                m_Context.TriggerAssetRenamePopup = true;
            }
            if (ImGui::MenuItem("Delete", "Delete")) {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                if (ec) {
                    PX_CORE_ERROR("Failed to delete asset: {0}", ec.message());
                }
                m_Context.SelectedAssetPath = "";
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Show in Explorer")) {
                #ifdef _WIN32
                std::string args = "/select,\"" + path.string() + "\"";
                std::replace(args.begin(), args.end(), '/', '\\');
                ShellExecuteA(NULL, "open", "explorer.exe", args.c_str(), NULL, SW_SHOWNORMAL);
                #endif
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginDragDropSource()) {
            std::string pathString = path.string();
            ImGui::SetDragDropPayload("ASSET_PATH", pathString.c_str(), pathString.size() + 1);
            ImGui::Text("Dragging %s", filename.c_str());
            ImGui::EndDragDropSource();
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 80.0f);
        ImGui::Text("%s", filename.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
    }

    bool AssetBrowserPanel::MatchAssetFilter(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        std::string filename = path.filename().string();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

        switch (m_Context.CurrentAssetFilter) {
            case AssetFilter::Textures:
                return (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga");
            case AssetFilter::Audio:
                return (ext == ".wav" || ext == ".ogg" || ext == ".mp3");
            case AssetFilter::Scripts:
                return (ext == ".cs");
            case AssetFilter::Prefabs:
                return (ext == ".prefab" || filename.ends_with(".prefab.json"));
            case AssetFilter::Scenes:
                return (ext == ".json" && !filename.ends_with(".prefab.json"));
            default:
                return true;
        }
    }

    bool AssetBrowserPanel::MatchAssetSearch(const std::filesystem::path& path, const std::string& query) {
        std::string filename = path.filename().string();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        return filename.find(query) != std::string::npos;
    }

}
