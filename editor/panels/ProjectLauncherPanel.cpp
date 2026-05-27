#include "ProjectLauncherPanel.hpp"
#include <imgui.h>
#include "editor/EditorIcons.hpp"
#include "editor/EditorContext.hpp"
#include <cstring>

namespace PixelEngine {

    void ProjectLauncherPanel::OnImGuiRender() {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBringToFrontOnFocus;
        
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(48.0f, 48.0f));
        ImGui::Begin("Project Launcher", nullptr, flags);
        ImGui::PopStyleVar();

        // 1. Header Section
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.38f, 0.35f, 0.90f, 1.0f)); // indigo accent
        ImGui::Text(ICON_FA_CUBES "  PIXEL ENGINE");
        ImGui::PopStyleColor();
        
        ImGui::SetWindowFontScale(1.1f);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Lightweight 2D-First Game Engine");
        ImGui::SetWindowFontScale(1.0f);
        
        ImGui::Dummy(ImVec2(0, 10.0f));
        
        // A subtle colored divider line
        ImVec2 lineStart = ImGui::GetCursorScreenPos();
        ImVec2 lineEnd = ImVec2(lineStart.x + ImGui::GetContentRegionAvail().x, lineStart.y);
        ImGui::GetWindowDrawList()->AddLine(lineStart, lineEnd, ImGui::GetColorU32(ImVec4(0.18f, 0.18f, 0.24f, 1.0f)), 2.0f);
        
        ImGui::Dummy(ImVec2(0, 24.0f));
        
        // 2. Columns Layout using Child Windows for separation
        float availWidth = ImGui::GetContentRegionAvail().x;
        float availHeight = ImGui::GetContentRegionAvail().y - 20.0f;
        float colWidth = (availWidth - 32.0f) * 0.5f;

        // --- LEFT COLUMN: Recent Projects ---
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.09f, 0.5f)); // slightly darker background for left panel
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 24.0f));
        
        ImGui::BeginChild("RecentProjectsCol", ImVec2(colWidth, availHeight), true, ImGuiWindowFlags_None);
        
        ImGui::Text(ICON_FA_SAVE "  Recent Projects");
        ImGui::Dummy(ImVec2(0, 12.0f));
        
        if (m_Context.RecentProjects.empty()) {
            ImGui::Dummy(ImVec2(0, 40.0f));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No recent projects found.");
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Create or open a project to get started!");
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.0f));
            
            for (size_t i = 0; i < m_Context.RecentProjects.size(); ++i) {
                const auto& proj = m_Context.RecentProjects[i];
                std::filesystem::path p(proj);
                std::string projName = p.filename().string();
                std::string parentPath = p.parent_path().string();
                
                ImGui::PushID(static_cast<int>(i));
                
                ImVec2 cardPos = ImGui::GetCursorScreenPos();
                ImVec2 cardSize = ImVec2(ImGui::GetContentRegionAvail().x, 68.0f);
                
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.18f, 0.18f, 0.26f, 1.0f)); // Custom hover background
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.14f, 0.14f, 0.20f, 1.0f));
                
                bool clicked = ImGui::Selectable(std::string("##card_" + std::to_string(i)).c_str(), false, ImGuiSelectableFlags_None, cardSize);
                
                ImGui::PopStyleColor(2);
                
                bool hovered = ImGui::IsItemHovered();
                
                if (!hovered) {
                    ImGui::GetWindowDrawList()->AddRectFilled(cardPos, ImVec2(cardPos.x + cardSize.x, cardPos.y + cardSize.y), ImGui::GetColorU32(ImVec4(0.11f, 0.11f, 0.14f, 1.0f)), 6.0f);
                }
                
                ImU32 borderColor = ImGui::GetColorU32(hovered ? ImVec4(0.38f, 0.35f, 0.90f, 0.8f) : ImVec4(0.18f, 0.18f, 0.24f, 1.0f));
                ImGui::GetWindowDrawList()->AddRect(cardPos, ImVec2(cardPos.x + cardSize.x, cardPos.y + cardSize.y), borderColor, 6.0f);
                
                ImVec2 namePos = ImVec2(cardPos.x + 16.0f, cardPos.y + 14.0f);
                ImVec2 pathPos = ImVec2(cardPos.x + 16.0f, cardPos.y + 38.0f);
                
                ImGui::GetWindowDrawList()->AddText(namePos, ImGui::GetColorU32(ImGuiCol_Text), std::string(ICON_FA_FOLDER "  " + projName).c_str());
                ImGui::GetWindowDrawList()->AddText(pathPos, ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 1.0f)), parentPath.c_str());
                
                if (clicked && m_Context.LoadProjectCallback) {
                    m_Context.LoadProjectCallback(proj);
                }
                
                ImGui::PopID();
                ImGui::Dummy(ImVec2(0, 10.0f)); // Spacing between cards
            }
            
            ImGui::PopStyleVar(2);
        }
        
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(); // Pop ChildBg
        
        ImGui::SameLine(0, 32.0f); // Spacing between columns

        // --- RIGHT COLUMN: Project Actions ---
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.07f, 0.07f, 0.09f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.0f, 24.0f));
        
        ImGui::BeginChild("ActionsCol", ImVec2(colWidth, availHeight), true, ImGuiWindowFlags_None);
        
        ImGui::Text(ICON_FA_GEAR "  Project Actions");
        ImGui::Dummy(ImVec2(0, 12.0f));
        
        if (ImGui::BeginTabBar("LauncherActionsTabBar", ImGuiTabBarFlags_None)) {
            
            // Tab 1: Create New Project
            if (ImGui::BeginTabItem(ICON_FA_PLUS "  Create Project")) {
                ImGui::Dummy(ImVec2(0, 16.0f));
                
                static char parentFolder[256] = "";
                static char newProjName[128] = "MyNewProject";
                
                if (parentFolder[0] == '\0') {
                    std::filesystem::path defaultPath = std::filesystem::current_path() / "projects";
                    strcpy_s(parentFolder, defaultPath.string().c_str());
                }
                
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Project Name");
                ImGui::InputText("##CreateProjName", newProjName, sizeof(newProjName));
                ImGui::Dummy(ImVec2(0, 10.0f));
                
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Parent Folder");
                ImGui::InputText("##CreateParentFolder", parentFolder, sizeof(parentFolder));
                ImGui::Dummy(ImVec2(0, 24.0f));
                
                // Style Indigo Create Button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.35f, 0.90f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.48f, 0.45f, 0.95f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.28f, 0.80f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 12.0f));
                
                if (ImGui::Button(ICON_FA_PLUS "  Create New Project", ImVec2(-1, 48)) && m_Context.CreateNewProjectCallback) {
                    m_Context.CreateNewProjectCallback(parentFolder, newProjName);
                }
                
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
                
                ImGui::EndTabItem();
            }
            
            // Tab 2: Open Existing Project
            if (ImGui::BeginTabItem(ICON_FA_FOLDER "  Open Project")) {
                ImGui::Dummy(ImVec2(0, 16.0f));
                
                static char openProjPath[256] = "";
                
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Project Folder or Path (.pixelproj)");
                ImGui::InputText("##OpenProjPath", openProjPath, sizeof(openProjPath));
                ImGui::Dummy(ImVec2(0, 24.0f));
                
                // Style Indigo Open Button
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.35f, 0.90f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.48f, 0.45f, 0.95f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.30f, 0.28f, 0.80f, 1.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 12.0f));
                
                if (ImGui::Button(ICON_FA_FOLDER "  Open Existing Project", ImVec2(-1, 48)) && m_Context.LoadProjectCallback) {
                    std::filesystem::path path(openProjPath);
                    if (std::filesystem::exists(path)) {
                        if (path.extension() == ".pixelproj") {
                            m_Context.LoadProjectCallback(path.parent_path().string());
                        } else if (std::filesystem::is_directory(path)) {
                            m_Context.LoadProjectCallback(path.string());
                        }
                    }
                }
                
                ImGui::PopStyleVar(2);
                ImGui::PopStyleColor(3);
                
                ImGui::EndTabItem();
            }
            
            ImGui::EndTabBar();
        }
        
        ImGui::EndChild();
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(); // Pop ChildBg
        
        ImGui::End();
    }

}
