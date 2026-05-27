#include "ProfilerPanel.hpp"
#include <imgui.h>
#include <engine/renderer/VulkanContext.hpp>
#include <engine/renderer/Renderer2D.hpp>
#include <engine/assets/AssetManager.hpp>

namespace PixelEngine {

    void ProfilerPanel::OnImGuiRender() {
        ImGui::Begin("Profiler");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("CPU Frame Time: %.3f ms", m_Context.DeltaTime * 1000.0f);
        
        if (m_Context.VulkanCtx) {
            ImGui::Text("GPU Frame Time: %.3f ms", m_Context.VulkanCtx->GetGPUTime());
        } else {
            ImGui::Text("GPU Frame Time: N/A");
        }
        
        auto stats = Renderer2D::GetStats();
        ImGui::Text("Batch Draw Calls: %u", stats.DrawCalls);
        ImGui::Text("Batch Quad Count: %u", stats.QuadCount);

        ImGui::Separator();
        ImGui::Text("ECS Runtime:");
        if (m_Context.ActiveScene) {
            ImGui::Text("Active Entities: %zu", m_Context.ActiveScene->Reg().storage<entt::entity>().size());
        } else {
            ImGui::Text("Active Entities: 0");
        }

        ImGui::Separator();
        ImGui::Text("Resource Lifetime Stats:");
        ImGui::Text("Loaded Textures: %zu", AssetManager::GetLoadedTexturesCount());
        ImGui::Text("Loaded Tilesets: %zu", AssetManager::GetLoadedTilesetsCount());
        ImGui::Text("Loaded SpriteSheets: %zu", AssetManager::GetLoadedSpriteSheetsCount());
        ImGui::Text("Loaded AudioClips: %zu", AssetManager::GetLoadedAudioClipsCount());
        
        ImGui::Separator();
        
        ImGui::Checkbox("Pixel Snapping", &m_Context.PixelSnapping);
        ImGui::SliderFloat("Rotation Speed", &m_Context.RotationSpeed, 0.0f, 200.0f);
        ImGui::End();
    }

}
