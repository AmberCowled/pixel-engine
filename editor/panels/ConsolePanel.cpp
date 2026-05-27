#include "ConsolePanel.hpp"
#include <imgui.h>
#include <engine/base/EditorConsoleSink.hpp>

namespace PixelEngine {

    void ConsolePanel::OnImGuiRender() {
        ImGui::Begin("Console");
        if (ImGui::Button("Clear")) {
            EditorConsoleSink::Clear();
        }
        ImGui::Separator();
        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        {
            const auto& logs = EditorConsoleSink::GetMessages();
            for (const auto& log : logs) {
                ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                if (log.Level == spdlog::level::warn) {
                    color = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
                } else if (log.Level == spdlog::level::err || log.Level == spdlog::level::critical) {
                    color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f);
                } else if (log.Level == spdlog::level::info) {
                    color = ImVec4(0.2f, 1.0f, 0.2f, 1.0f);
                }
                ImGui::TextColored(color, "%s", log.Message.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
        ImGui::EndChild();
        ImGui::End();
    }

}
