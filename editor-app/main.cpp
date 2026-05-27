#include <engine/core/EngineApp.hpp>
#include <engine/base/Log.hpp>
#include <engine/base/EditorConsoleSink.hpp>
#include <engine/renderer/VulkanContext.hpp>
#include <engine/renderer/GraphicsPipeline.hpp>
#include <engine/renderer/Buffer.hpp>
#include <engine/renderer/Vertex.hpp>
#include <engine/renderer/UBO.hpp>
#include <engine/renderer/CubeData.hpp>
#include <engine/renderer/QuadData.hpp>
#include <engine/renderer/OffscreenTarget.hpp>
#include <engine/renderer/Camera.hpp>
#include <engine/renderer/Texture.hpp>
#include <engine/renderer/Renderer2D.hpp>
#include <engine/renderer/ShaderHotReloader.hpp>
#include <engine/assets/AssetManager.hpp>
#include <engine/assets/AssetWatcher.hpp>
#include <engine/assets/AudioClip.hpp>
#include <engine/audio/AudioManager.hpp>
#include <engine/ecs/Scene.hpp>
#include <engine/ecs/Entity.hpp>
#include <engine/ecs/Components.hpp>
#include <engine/ecs/RenderSystem.hpp>
#include <engine/ecs/SceneSerializer.hpp>
#include <engine/scripting/ScriptEngine.hpp>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_vulkan.h>
#include <imgui_impl_sdl3.h>
#include <ImGuizmo.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <chrono>
#include <iostream>
#include <array>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <unordered_set>
#include <unordered_map>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>
#endif
#include <thread>
#include <atomic>

#include "editor/EditorContext.hpp"
#include "editor/EditorCommands.hpp"
#include "editor/panels/AnimationEditorPanel.hpp"
#include "editor/panels/AssetBrowserPanel.hpp"
#include "editor/panels/AudioMixerPanel.hpp"
#include "editor/panels/ConsolePanel.hpp"
#include "editor/panels/HierarchyPanel.hpp"
#include "editor/panels/InspectorPanel.hpp"
#include "editor/panels/ProfilerPanel.hpp"
#include "editor/panels/ProjectLauncherPanel.hpp"
#include "editor/panels/TilemapPaintingPanel.hpp"
#include "editor/panels/ToolbarPanel.hpp"
#include "editor/panels/ViewportPanel.hpp"
#include "editor/EditorIcons.hpp"
#include "editor/EditorUtils.hpp"

class EditorApp : public PixelEngine::EngineApp {
public:
    EditorApp() : PixelEngine::EngineApp({"Pixel Editor Workspace", 1280, 720}) {
        PX_INFO("Editor Workspace Started.");
        
        auto& context = GetVulkanContext();

        // 1. Initialize global runtimes (Renderer2D, Audio)
        PixelEngine::Renderer2D::Init(context);
        PixelEngine::AudioManager::Init();

        // Initialize EditorContext
        m_Context.VulkanCtx = &context;
        m_Context.Window = m_Window;
        m_Context.ActiveScene = std::make_shared<PixelEngine::Scene>();
        m_Context.EditorScene = m_Context.ActiveScene;
        m_Context.CurrentSceneState = PixelEngine::SceneState::Edit;
        m_Context.ProjectLoaded = false;

        // Register callbacks
        m_Context.LoadProjectCallback = [this](const std::string& path) { LoadProject(path); };
        m_Context.CreateNewProjectCallback = [this](const std::string& parentFolder, const std::string& name) { CreateNewProject(parentFolder, name); };
        m_Context.InstantiatePrefabCallback = [this](const std::string& path) { InstantiatePrefab(path); };
        m_Context.SaveEntityAsPrefabCallback = [this](PixelEngine::UUID uuid, const std::filesystem::path& dir) { SaveEntityAsPrefab(uuid, dir); };
        m_Context.CompileCSProjectAsyncCallback = [this]() { CompileCSProjectAsync(); };
        m_Context.GroupEntityCallback = [this](PixelEngine::Entity ent) { GroupEntity(ent); };
        m_Context.ReorderEntityCallback = [this](PixelEngine::Entity ent, bool up) { ReorderEntity(ent, up); };
        m_Context.CreatePresetEntityCallback = [this](const std::string& preset, PixelEngine::UUID parent) { return CreatePresetEntity(preset, parent); };
        m_Context.DuplicateSubtreeCallback = [this](PixelEngine::Entity ent, PixelEngine::UUID parent, const std::string& suffix) { return DuplicateSubtree(ent, parent, suffix); };
        m_Context.DeleteEntityCallback = [this](PixelEngine::Entity ent) { DeleteEntity(ent); };
        m_Context.ApplyPrefabOverridesCallback = [this](PixelEngine::Entity ent, PixelEngine::UUID prefab) { ApplyPrefabOverrides(ent, prefab); };
        m_Context.RevertPrefabOverridesCallback = [this](PixelEngine::Entity ent, PixelEngine::UUID prefab) { RevertPrefabOverrides(ent, prefab); };

        // Setup panels list
        m_Panels.push_back(std::make_unique<PixelEngine::ToolbarPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::HierarchyPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::InspectorPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::ViewportPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::AssetBrowserPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::ConsolePanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::ProfilerPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::AudioMixerPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::TilemapPaintingPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::AnimationEditorPanel>(m_Context));
        m_Panels.push_back(std::make_unique<PixelEngine::ProjectLauncherPanel>(m_Context));

        // Load recent projects configuration
        LoadRecentProjects();

        // 2. Create Geometry Buffers for Fullscreen Upscale
        VkDeviceSize quadVertexBufferSize = sizeof(PixelEngine::FULLSCREEN_TRIANGLE_VERTICES[0]) * PixelEngine::FULLSCREEN_TRIANGLE_VERTICES.size();
        m_QuadVertexBuffer = std::make_unique<PixelEngine::Buffer>(
            context, quadVertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        m_QuadVertexBuffer->Map();
        m_QuadVertexBuffer->WriteToBuffer((void*)PixelEngine::FULLSCREEN_TRIANGLE_VERTICES.data(), quadVertexBufferSize);

        VkDeviceSize quadIndexBufferSize = sizeof(PixelEngine::FULLSCREEN_TRIANGLE_INDICES[0]) * PixelEngine::FULLSCREEN_TRIANGLE_INDICES.size();
        m_QuadIndexBuffer = std::make_unique<PixelEngine::Buffer>(
            context, quadIndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        m_QuadIndexBuffer->Map();
        m_QuadIndexBuffer->WriteToBuffer((void*)PixelEngine::FULLSCREEN_TRIANGLE_INDICES.data(), quadIndexBufferSize);

        // 3. Create Upscale Pipeline
        PX_INFO("EditorApp: Searching for upscale shaders...");
        std::vector<std::string> upscaleSearchPaths = {
            "shaders/",
            "../shaders/",
            "../../shaders/",
            "build/bin/shaders/",
            "../bin/shaders/"
        };
        std::string upscaleVert, upscaleFrag;
        for (const auto& p : upscaleSearchPaths) {
            if (upscaleVert.empty() && std::filesystem::exists(p + "upscale.vert.spv")) upscaleVert = p + "upscale.vert.spv";
            if (upscaleFrag.empty() && std::filesystem::exists(p + "upscale.frag.spv")) upscaleFrag = p + "upscale.frag.spv";
        }
        if (upscaleVert.empty() || upscaleFrag.empty()) PX_CORE_CRITICAL("Failed to find Upscale shaders!");
        PX_INFO("EditorApp: Loading upscale shaders. Vert: {0}, Frag: {1}", upscaleVert, upscaleFrag);

        PixelEngine::PipelineConfigInfo upscaleConfig{};
        PixelEngine::GraphicsPipeline::DefaultPipelineConfigInfo(upscaleConfig);
        upscaleConfig.renderPass = context.GetRenderPass();
        upscaleConfig.pipelineLayout = context.GetUpscalePipelineLayout();
        upscaleConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
        upscaleConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        upscaleConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        PX_INFO("EditorApp: Creating upscale pipeline...");
        m_UpscalePipeline = std::make_unique<PixelEngine::GraphicsPipeline>(
            context, upscaleVert, upscaleFrag, upscaleConfig
        );

        // 4. Initialize ECS Systems
        m_RenderSystem = std::make_unique<PixelEngine::RenderSystem>(context);
        PX_INFO("EditorApp: Constructor complete!");
    }

    ~EditorApp() {
        auto& context = GetVulkanContext();
        vkDeviceWaitIdle(context.GetDevice());

        if (m_Context.ViewportDescriptorSet != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(m_Context.ViewportDescriptorSet);
            m_Context.ViewportDescriptorSet = VK_NULL_HANDLE;
        }

        PixelEngine::Renderer2D::Shutdown();
        PixelEngine::AssetManager::Shutdown();
        PixelEngine::AudioManager::Shutdown();
        PX_INFO("Sandbox App Shutdown.");
    }

    void OnUpdate(float deltaTime) override {
        m_Context.DeltaTime = deltaTime;

        // Validate and clear selected entity if it has been destroyed
        if ((entt::entity)m_Context.SelectedEntity != entt::null) {
            if (!m_Context.ActiveScene || !m_Context.ActiveScene->Reg().valid(m_Context.SelectedEntity)) {
                m_Context.SelectedEntity = {};
            }
        }

        // Draw warning modals for unsaved changes
        DrawExitPopup();
        DrawCloseProjectPopup();
        DrawRenameEntityPopup();
        DrawRenameAssetPopup();
        DrawCreateFolderPopup();
        DrawCreateScriptPopup();
        DrawCreateMetadataPopup();

        if (m_Context.AssemblyReloadPending.load()) {
            m_Context.AssemblyReloadPending = false;
            PX_CORE_INFO("Reloading Game Assembly...");
            std::filesystem::path projectPath(m_Context.ProjectPath);
            std::string assemblyPath = "bin/UserGame.dll";
            if (std::filesystem::exists(projectPath / assemblyPath)) {
                PixelEngine::ScriptEngine::LoadGameAssembly((projectPath / assemblyPath).string());
                
                auto scriptView = m_Context.ActiveScene->Reg().view<PixelEngine::IDComponent>();
                for (auto entityID : scriptView) {
                    PixelEngine::Entity entity = { entityID, m_Context.ActiveScene.get() };
                    if (entity.HasComponent<PixelEngine::ScriptComponent>()) {
                        PixelEngine::ScriptEngine::OnCreateEntity(entity);
                    }
                }
                PX_CORE_INFO("Game Assembly reloaded successfully.");
            }
        }

        if (!m_Context.ProjectLoaded) {
            // Only draw launcher
            for (auto& panel : m_Panels) {
                if (dynamic_cast<PixelEngine::ProjectLauncherPanel*>(panel.get())) {
                    panel->OnImGuiRender();
                }
            }
            return;
        }

        // Update window title dirty state indicator
        static bool lastDirty = false;
        bool currentDirty = PixelEngine::EditorHistory::IsDirty();
        if (currentDirty != lastDirty) {
            std::string title = "Pixel Editor Workspace - " + m_Context.ProjectName;
            if (currentDirty) {
                title += " *";
            }
            SDL_SetWindowTitle(m_Window, title.c_str());
            lastDirty = currentDirty;
        }

        // Handle keyboard shortcuts (when not typing in a text field)
        ImGuiIO& io_shortcuts = ImGui::GetIO();
        if (!io_shortcuts.WantTextInput && m_Context.CurrentSceneState == PixelEngine::SceneState::Edit) {
            if (io_shortcuts.KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
                    PixelEngine::UUID selectedUUID = m_Context.SelectedEntity ? m_Context.SelectedEntity.GetComponent<PixelEngine::IDComponent>().ID : PixelEngine::UUID(0);
                    PixelEngine::EditorHistory::Undo();
                    m_Context.SelectedEntity = (selectedUUID != PixelEngine::UUID(0)) ? m_Context.ActiveScene->GetEntityByUUID(selectedUUID) : PixelEngine::Entity{};
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
                    PixelEngine::UUID selectedUUID = m_Context.SelectedEntity ? m_Context.SelectedEntity.GetComponent<PixelEngine::IDComponent>().ID : PixelEngine::UUID(0);
                    PixelEngine::EditorHistory::Redo();
                    m_Context.SelectedEntity = (selectedUUID != PixelEngine::UUID(0)) ? m_Context.ActiveScene->GetEntityByUUID(selectedUUID) : PixelEngine::Entity{};
                }
                // Ctrl+Shift+N: Create Empty Entity
                if (io_shortcuts.KeyShift && ImGui::IsKeyPressed(ImGuiKey_N)) {
                    PixelEngine::UUID parentUUID = m_Context.SelectedEntity ? m_Context.SelectedEntity.GetComponent<PixelEngine::IDComponent>().ID : PixelEngine::UUID(0);
                    CreatePresetEntity("Empty Entity", parentUUID);
                }
                // Ctrl+D: Duplicate Selected Entity
                if (ImGui::IsKeyPressed(ImGuiKey_D)) {
                    if (m_Context.SelectedEntity) {
                        m_Context.SelectedEntity = DuplicateSubtree(m_Context.SelectedEntity, 0, " (Copy)");
                    }
                }
            } else {
                // F2: Rename
                if (ImGui::IsKeyPressed(ImGuiKey_F2)) {
                    if (m_Context.HierarchyFocused && m_Context.SelectedEntity) {
                        m_Context.TriggerRenamePopup = true;
                    } else if (m_Context.AssetBrowserFocused && !m_Context.SelectedAssetPath.empty()) {
                        m_Context.TriggerAssetRenamePopup = true;
                    }
                }
                // Delete: Delete
                if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                    if (m_Context.HierarchyFocused && m_Context.SelectedEntity) {
                        PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
                        nlohmann::json beforeState = serializer.SerializeToJson();
                        
                        DeleteEntity(m_Context.SelectedEntity);
                        m_Context.SelectedEntity = {};

                        nlohmann::json afterState = serializer.SerializeToJson();
                        PixelEngine::EditorHistory::PushCommand(
                            std::make_unique<PixelEngine::SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Delete Entity")
                        );
                    } else if (m_Context.AssetBrowserFocused && !m_Context.SelectedAssetPath.empty()) {
                        std::error_code ec;
                        if (std::filesystem::is_directory(m_Context.SelectedAssetPath)) {
                            std::filesystem::remove_all(m_Context.SelectedAssetPath, ec);
                        } else {
                            std::filesystem::remove(m_Context.SelectedAssetPath, ec);
                        }
                        m_Context.SelectedAssetPath = "";
                    }
                }
            }
        }

        // Start ImGuizmo frame
        ImGuizmo::BeginFrame();

        // Run Asset Watcher
        PixelEngine::AssetWatcher::Update();

        // Update ECS Systems if Playing
        if (m_Context.CurrentSceneState == PixelEngine::SceneState::Play) {
            m_Context.ActiveScene->OnUpdate(deltaTime);
            PixelEngine::ScriptEngine::OnUpdate(deltaTime);
        } else if (m_Context.CurrentSceneState == PixelEngine::SceneState::Edit) {
            if (m_Context.SelectedEntity && m_Context.SelectedEntity.HasComponent<PixelEngine::AnimatorComponent>() && m_Context.SelectedEntity.HasComponent<PixelEngine::SpriteRendererComponent>()) {
                auto& ac = m_Context.SelectedEntity.GetComponent<PixelEngine::AnimatorComponent>();
                auto& sprite = m_Context.SelectedEntity.GetComponent<PixelEngine::SpriteRendererComponent>();
                if (ac.Playing && !ac.CurrentClip.empty()) {
                    PixelEngine::AnimationClip* clip = nullptr;
                    for (auto& c : ac.Clips) {
                        if (c.Name == ac.CurrentClip) {
                            clip = &c;
                            break;
                        }
                    }
                    if (clip && !clip->Frames.empty()) {
                        float frameTime = 1.0f / clip->FPS;
                        ac.Timer += deltaTime;
                        if (ac.Timer >= frameTime) {
                            ac.Timer -= frameTime;
                            ac.CurrentFrame++;
                            if (ac.CurrentFrame >= static_cast<int>(clip->Frames.size())) {
                                if (clip->Loop) {
                                    ac.CurrentFrame = 0;
                                } else {
                                    ac.CurrentFrame = static_cast<int>(clip->Frames.size()) - 1;
                                    ac.Playing = false;
                                }
                            }
                        }
                        
                        auto spritesheet = PixelEngine::AssetManager::GetSpriteSheet(ac.SpriteSheetID);
                        if (spritesheet) {
                            sprite.Mat.TextureID = spritesheet->TextureID;
                            auto fit = spritesheet->Frames.find(clip->Frames[ac.CurrentFrame].FrameName);
                            if (fit != spritesheet->Frames.end()) {
                                sprite.Mat.UVs = fit->second.UVs;
                            }
                        }
                    }
                }
            }
        }

        m_Context.Rotation += deltaTime * m_Context.RotationSpeed;

        // Keyboard shortcuts for Gizmos
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && m_Context.CurrentSceneState == PixelEngine::SceneState::Edit) {
            if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_Context.GizmoType = -1;
            if (ImGui::IsKeyPressed(ImGuiKey_W)) m_Context.GizmoType = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) m_Context.GizmoType = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_Context.GizmoType = ImGuizmo::SCALE;
        }

        // Dockspace Root
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* imgui_viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(imgui_viewport->WorkPos);
            ImGui::SetNextWindowSize(imgui_viewport->WorkSize);
            ImGui::SetNextWindowViewport(imgui_viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        if (!opt_padding)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Editor Workspace", nullptr, window_flags);
        if (!opt_padding)
            ImGui::PopStyleVar();

        if (opt_fullscreen)
            ImGui::PopStyleVar(2);

        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

            if (m_Context.ResetLayout || !ImGui::DockBuilderGetNode(dockspace_id)) {
                m_Context.ResetLayout = false;
                
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
                
                const ImGuiViewport* imgui_viewport = ImGui::GetMainViewport();
                ImGui::DockBuilderSetNodeSize(dockspace_id, imgui_viewport->WorkSize);

                ImGuiID dock_main_id = dockspace_id;
                
                ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.06f, nullptr, &dock_main_id);
                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
                ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
                ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.30f, nullptr, &dock_main_id);

                ImGui::DockBuilderDockWindow("Toolbar", dock_id_top);
                ImGui::DockBuilderDockWindow("Hierarchy", dock_id_left);
                ImGui::DockBuilderDockWindow("Tilemap Painting", dock_id_left);
                ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
                ImGui::DockBuilderDockWindow("Profiler", dock_id_right);
                ImGui::DockBuilderDockWindow("Asset Browser", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Animation Editor", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Audio Mixer", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Viewport", dock_main_id);

                ImGui::DockBuilderFinish(dockspace_id);
            }

            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        // Main Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem(ICON_FA_SAVE " Save Scene", "Ctrl+S")) {
                    std::filesystem::path projectPath(m_Context.ProjectPath);
                    std::filesystem::path scenePath = projectPath / (m_Context.ActiveScenePath.empty() ? "assets/scenes/startup.json" : m_Context.ActiveScenePath);
                    std::filesystem::create_directories(scenePath.parent_path());
                    PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
                    serializer.Serialize(scenePath.string());
                    PixelEngine::EditorHistory::SetDirty(false);
                    PX_INFO("Scene saved to: %s", scenePath.string().c_str());
                }
                if (ImGui::MenuItem("Close Project")) {
                    if (PixelEngine::EditorHistory::IsDirty()) {
                        m_Context.ShowCloseProjectPopup = true;
                    } else {
                        m_Context.ProjectLoaded = false;
                        m_Context.ProjectPath = "";
                        m_Context.ProjectName = "";
                        m_Context.ActiveScenePath = "";
                        SDL_SetWindowTitle(m_Window, "Pixel Editor Workspace");
                    }
                }
                if (ImGui::MenuItem("Load Scene")) {
                    PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
                    if (serializer.Deserialize("assets/scenes/sandbox_scene.json")) {
                        m_Context.SelectedEntity = {};
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit")) {
                    Close();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Reset Layout")) {
                    m_Context.ResetLayout = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Game Object")) {
                if (ImGui::MenuItem("Create Empty Entity")) {
                    m_Context.ActiveScene->CreateEntity("Empty Entity");
                }
                if (ImGui::MenuItem("Create Sprite Entity")) {
                    auto e = m_Context.ActiveScene->CreateEntity("Sprite Entity");
                    e.AddComponent<PixelEngine::SpriteRendererComponent>();
                    e.GetComponent<PixelEngine::SpriteRendererComponent>().Mat.TextureID = m_Context.TestTexture;
                }
                if (ImGui::MenuItem("Create Cube Entity")) {
                    auto e = m_Context.ActiveScene->CreateEntity("Cube Entity");
                    e.AddComponent<PixelEngine::MeshRendererComponent>();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Render modular panels in loop
        for (auto& panel : m_Panels) {
            if (dynamic_cast<PixelEngine::ProjectLauncherPanel*>(panel.get())) {
                continue; // Project launcher is not drawn inside dockspace
            }
            panel->OnImGuiRender();
        }

        ImGui::End(); // Editor Workspace

        // Update Camera Projection Matrix
        if (m_Context.ProjectLoaded && m_Context.OffscreenBuffer) {
            float aspect = m_Context.OffscreenBuffer->GetWidth() / (float)m_Context.OffscreenBuffer->GetHeight();
            m_Context.EditorCamera.SetPerspectiveProjection(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        }
    }

    void OnRender() override {
        uint32_t imageIndex = GetCurrentImageIndex();
        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        auto& context = GetVulkanContext();

        // Pass 1: Offscreen ECS Render (only if project is loaded)
        if (m_Context.ProjectLoaded && m_Context.OffscreenBuffer) {
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = context.GetOffscreenRenderPass();
            renderPassInfo.framebuffer = m_Context.OffscreenBuffer->GetFramebuffer();
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = { m_Context.OffscreenBuffer->GetWidth(), m_Context.OffscreenBuffer->GetHeight() };

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)m_Context.OffscreenBuffer->GetWidth();
            viewport.height = (float)m_Context.OffscreenBuffer->GetHeight();
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = { m_Context.OffscreenBuffer->GetWidth(), m_Context.OffscreenBuffer->GetHeight() };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Delegate rendering to ECS system
            m_RenderSystem->Render(commandBuffer, imageIndex, *m_Context.ActiveScene, m_Context.EditorCamera);

            vkCmdEndRenderPass(commandBuffer);
        }

        // Pass 2: Upscale to Swapchain (includes UI)
        {
            this->BeginSwapChainRenderPass(commandBuffer, imageIndex, {0.0f, 0.0f, 0.0f, 1.0f});

            auto extent = context.GetSwapchainExtent();
            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)extent.width;
            viewport.height = (float)extent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = extent;
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            if (m_Context.ProjectLoaded) {
                m_UpscalePipeline->Bind(commandBuffer);

                VkBuffer vertexBuffers[] = {m_QuadVertexBuffer->GetBuffer()};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
                vkCmdBindIndexBuffer(commandBuffer, m_QuadIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

                VkDescriptorSet upscaleDescriptorSet = context.GetUpscaleDescriptorSet(imageIndex);
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.GetUpscalePipelineLayout(), 0, 1, &upscaleDescriptorSet, 0, nullptr);

                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(PixelEngine::FULLSCREEN_TRIANGLE_INDICES.size()), 1, 0, 0, 0);
            }
        }
    }

    bool OnCloseRequested() override {
        if (m_Context.ProjectLoaded && PixelEngine::EditorHistory::IsDirty()) {
            m_Context.ShowExitPopup = true;
            return false;
        }
        return true;
    }

    void DrawExitPopup() {
        if (m_Context.ShowExitPopup) {
            ImGui::OpenPopup("Save Changes?");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        
        if (ImGui::BeginPopupModal("Save Changes?", &m_Context.ShowExitPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes. Do you want to save before exiting?");
            ImGui::Separator();

            if (ImGui::Button(ICON_FA_SAVE " Save", ImVec2(120, 0))) {
                std::filesystem::path projectPath(m_Context.ProjectPath);
                std::filesystem::path scenePath = projectPath / (m_Context.ActiveScenePath.empty() ? "assets/scenes/startup.json" : m_Context.ActiveScenePath);
                PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
                serializer.Serialize(scenePath.string());
                PixelEngine::EditorHistory::SetDirty(false);
                
                m_Context.ShowExitPopup = false;
                Close();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
                m_Context.ShowExitPopup = false;
                PixelEngine::EditorHistory::SetDirty(false);
                Close();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_Context.ShowExitPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawCloseProjectPopup() {
        if (m_Context.ShowCloseProjectPopup) {
            ImGui::OpenPopup("Save Project Changes?");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        
        if (ImGui::BeginPopupModal("Save Project Changes?", &m_Context.ShowCloseProjectPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes. Do you want to save before closing the project?");
            ImGui::Separator();

            if (ImGui::Button(ICON_FA_SAVE " Save", ImVec2(120, 0))) {
                std::filesystem::path projectPath(m_Context.ProjectPath);
                std::filesystem::path scenePath = projectPath / (m_Context.ActiveScenePath.empty() ? "assets/scenes/startup.json" : m_Context.ActiveScenePath);
                PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
                serializer.Serialize(scenePath.string());
                PixelEngine::EditorHistory::SetDirty(false);
                
                m_Context.ShowCloseProjectPopup = false;
                m_Context.ProjectLoaded = false;
                m_Context.ProjectPath = "";
                m_Context.ProjectName = "";
                m_Context.ActiveScenePath = "";
                SDL_SetWindowTitle(m_Window, "Pixel Editor Workspace");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
                m_Context.ShowCloseProjectPopup = false;
                PixelEngine::EditorHistory::SetDirty(false);
                m_Context.ProjectLoaded = false;
                m_Context.ProjectPath = "";
                m_Context.ProjectName = "";
                m_Context.ActiveScenePath = "";
                SDL_SetWindowTitle(m_Window, "Pixel Editor Workspace");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_Context.ShowCloseProjectPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawRenameEntityPopup() {
        if (m_Context.TriggerRenamePopup) {
            ImGui::OpenPopup("Rename Entity");
            m_Context.TriggerRenamePopup = false;
        }

        if (ImGui::BeginPopupModal("Rename Entity", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char renameBuf[256] = "";
            if (ImGui::IsWindowAppearing()) {
                if (m_Context.SelectedEntity) {
                    strcpy_s(renameBuf, sizeof(renameBuf), m_Context.SelectedEntity.GetComponent<PixelEngine::TagComponent>().Tag.c_str());
                } else {
                    renameBuf[0] = '\0';
                }
            }
            
            ImGui::Text("Enter new name:");
            ImGui::InputText("##newName", renameBuf, sizeof(renameBuf));
            
            if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (m_Context.SelectedEntity && renameBuf[0] != '\0') {
                    PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
                    nlohmann::json beforeState = serializer.SerializeToJson();
                    
                    m_Context.SelectedEntity.GetComponent<PixelEngine::TagComponent>().Tag = renameBuf;
                    PixelEngine::TrackOverride(m_Context.SelectedEntity, "TagComponent.Tag");
                    
                    nlohmann::json afterState = serializer.SerializeToJson();
                    PixelEngine::EditorHistory::PushCommand(
                        std::make_unique<PixelEngine::SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Rename Entity")
                    );
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawRenameAssetPopup() {
        if (m_Context.TriggerAssetRenamePopup) {
            ImGui::OpenPopup("Rename Asset");
            m_Context.TriggerAssetRenamePopup = false;
        }

        if (ImGui::BeginPopupModal("Rename Asset", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char assetRenameBuf[256] = "";
            if (ImGui::IsWindowAppearing()) {
                strcpy_s(assetRenameBuf, sizeof(assetRenameBuf), m_Context.SelectedAssetPath.filename().string().c_str());
            }
            
            ImGui::Text("Enter new name:");
            ImGui::InputText("##newAssetName", assetRenameBuf, sizeof(assetRenameBuf));
            
            if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (assetRenameBuf[0] != '\0') {
                    auto newPath = m_Context.SelectedAssetPath.parent_path() / assetRenameBuf;
                    std::error_code ec;
                    std::filesystem::rename(m_Context.SelectedAssetPath, newPath, ec);
                    if (ec) {
                        PX_CORE_ERROR("Failed to rename asset: {0}", ec.message());
                    } else {
                        m_Context.SelectedAssetPath = newPath;
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawCreateFolderPopup() {
        if (m_Context.TriggerCreateFolderPopup) {
            ImGui::OpenPopup("Create Folder");
            m_Context.TriggerCreateFolderPopup = false;
        }

        if (ImGui::BeginPopupModal("Create Folder", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char folderNameBuf[256] = "";
            if (ImGui::IsWindowAppearing()) {
                folderNameBuf[0] = '\0';
            }
            
            ImGui::Text("Folder Name:");
            ImGui::InputText("##folderName", folderNameBuf, sizeof(folderNameBuf));
            
            if (ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (folderNameBuf[0] != '\0' && !m_Context.CurrentDirectory.empty()) {
                    std::filesystem::path newFolder = m_Context.CurrentDirectory / folderNameBuf;
                    std::error_code ec;
                    std::filesystem::create_directories(newFolder, ec);
                    if (ec) {
                        PX_CORE_ERROR("Failed to create folder: {0}", ec.message());
                    } else {
                        PX_CORE_INFO("Created folder: {0}", newFolder.string());
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawCreateScriptPopup() {
        if (m_Context.TriggerCreateScriptPopup) {
            ImGui::OpenPopup("Create C# Script");
            m_Context.TriggerCreateScriptPopup = false;
        }

        if (ImGui::BeginPopupModal("Create C# Script", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char scriptNameBuf[256] = "";
            if (ImGui::IsWindowAppearing()) {
                scriptNameBuf[0] = '\0';
            }
            
            ImGui::Text("Class / File Name:");
            ImGui::InputText("##scriptName", scriptNameBuf, sizeof(scriptNameBuf));
            
            if (ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (scriptNameBuf[0] != '\0') {
                    std::string className = scriptNameBuf;
                    className.erase(std::remove_if(className.begin(), className.end(), [](char c) {
                        return !std::isalnum(c) && c != '_';
                    }), className.end());

                    if (!className.empty() && m_Context.ProjectLoaded) {
                        std::filesystem::path targetDir = m_Context.CurrentDirectory.empty() ? std::filesystem::path(m_Context.ProjectPath) : m_Context.CurrentDirectory;
                        std::filesystem::create_directories(targetDir);
                        std::filesystem::path scriptPath = targetDir / (className + ".cs");

                        std::ofstream scriptOut(scriptPath);
                        if (scriptOut.is_open()) {
                            scriptOut << "using PixelEngine;\n\n";
                            scriptOut << "public class " << className << " : MonoBehaviour {\n";
                            scriptOut << "    public override void OnCreate() {\n";
                            scriptOut << "        Log.Info(\"" << className << " script OnCreate called!\");\n";
                            scriptOut << "    }\n\n";
                            scriptOut << "    public override void OnUpdate(float dt) {\n";
                            scriptOut << "        // Add logic here\n";
                            scriptOut << "    }\n";
                            scriptOut << "}\n";
                            scriptOut.close();
                            
                            PX_CORE_INFO("Created C# script template: {0}", scriptPath.string());
                            CompileCSProjectAsync();
                        } else {
                            PX_CORE_ERROR("Failed to write C# script to {0}", scriptPath.string());
                        }
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawCreateMetadataPopup() {
        if (m_Context.TriggerCreateMetadataPopup) {
            ImGui::OpenPopup("Create Metadata File");
            m_Context.TriggerCreateMetadataPopup = false;
        }

        if (ImGui::BeginPopupModal("Create Metadata File", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char metaNameBuf[256] = "";
            static int metaTypeIndex = 0;
            if (ImGui::IsWindowAppearing()) {
                metaNameBuf[0] = '\0';
            }
            
            ImGui::Text("File Name (without extension):");
            ImGui::InputText("##metaName", metaNameBuf, sizeof(metaNameBuf));
            ImGui::Combo("Type", &metaTypeIndex, "Tileset (.tileset)\0SpriteSheet (.spritesheet)\0");
            
            if (ImGui::Button("Create") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (metaNameBuf[0] != '\0' && !m_Context.CurrentDirectory.empty()) {
                    std::string filename = metaNameBuf;
                    std::filesystem::path filePath;
                    if (metaTypeIndex == 0) {
                        filePath = m_Context.CurrentDirectory / (filename + ".tileset");
                        std::ofstream fout(filePath);
                        if (fout.is_open()) {
                            nlohmann::json tsJson = {
                                {"tileSize", 16},
                                {"texturePath", "assets/test.png"}
                            };
                            fout << tsJson.dump(4) << std::endl;
                            fout.close();
                            PX_CORE_INFO("Created tileset metadata template: {0}", filePath.string());
                        }
                    } else {
                        filePath = m_Context.CurrentDirectory / (filename + ".spritesheet");
                        std::ofstream fout(filePath);
                        if (fout.is_open()) {
                            nlohmann::json ssJson = {
                                {"texturePath", "assets/test.png"},
                                {"frames", {
                                    {"idle_0", {{"x", 0.0}, {"y", 0.0}, {"w", 16.0}, {"h", 16.0}}}
                                }}
                            };
                            fout << ssJson.dump(4) << std::endl;
                            fout.close();
                            PX_CORE_INFO("Created spritesheet metadata template: {0}", filePath.string());
                        }
                    }
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

private:
    std::filesystem::path GetExecutableDirectory() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        return std::filesystem::path(path).parent_path();
    }

public:
    void LoadProject(const std::string& projectPathStr) {
        auto& context = GetVulkanContext();
        
        m_Context.ProjectPath = projectPathStr;
        std::filesystem::path projectPath(m_Context.ProjectPath);
        m_Context.ProjectName = projectPath.filename().string();
        
        // 1. Set up Assets Root
        std::filesystem::path assetsPath = projectPath / "assets";
        std::filesystem::create_directories(assetsPath);
        PixelEngine::AssetManager::Init(context, assetsPath.string());
        m_Context.CurrentDirectory = assetsPath;

        // 2. Set up shaders
        std::filesystem::path shadersPath = projectPath / "shaders";
        if (!std::filesystem::exists(shadersPath)) {
            shadersPath = std::filesystem::absolute("shaders");
        }
        PixelEngine::ShaderHotReloader::Init(shadersPath.string());
        
        // 3. Set up Asset Watcher
        PixelEngine::AssetWatcher::Init(assetsPath.string());

        // 4. Initialize Scene
        m_Context.EditorScene = std::make_shared<PixelEngine::Scene>();
        m_Context.ActiveScene = m_Context.EditorScene;
        m_Context.SelectedEntity = {};
        
        // 5. Load Startup Scene if it exists
        std::filesystem::path projFilePath = projectPath / (m_Context.ProjectName + ".pixelproj");
        std::string startupScenePath = "";
        if (std::filesystem::exists(projFilePath)) {
            std::ifstream fin(projFilePath);
            if (fin.is_open()) {
                try {
                    nlohmann::json projJson;
                    fin >> projJson;
                    startupScenePath = projJson.value("StartupScene", "");
                } catch (...) {}
                    fin.close();
            }
        }
        
        if (!startupScenePath.empty() && std::filesystem::exists(projectPath / startupScenePath)) {
            m_Context.ActiveScenePath = startupScenePath;
            PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
            serializer.Deserialize((projectPath / startupScenePath).string());
        } else {
            m_Context.ActiveScenePath = "assets/scenes/startup.json";
            
            // Generate default scene assets and entity
            m_Context.TestTexture = 0;
            std::filesystem::path defaultPng = projectPath / "assets/test.png";
            if (std::filesystem::exists(defaultPng)) {
                m_Context.TestTexture = PixelEngine::AssetManager::LoadTexture(defaultPng.string());
            }

            // Textured Cube Entity
            auto cube = m_Context.EditorScene->CreateEntity("Textured Cube");
            cube.AddComponent<PixelEngine::MeshRendererComponent>();
            cube.GetComponent<PixelEngine::MeshRendererComponent>().TextureID = m_Context.TestTexture;

            // Floating Sprite Entity
            auto sprite = m_Context.EditorScene->CreateEntity("Floating Sprite");
            sprite.AddComponent<PixelEngine::SpriteRendererComponent>();
            sprite.GetComponent<PixelEngine::SpriteRendererComponent>().Mat.TextureID = m_Context.TestTexture;
            sprite.GetComponent<PixelEngine::TransformComponent>().Translation = {2.0f, 0.0f, 0.0f};
        }

        // 6. Initialize ScriptEngine
        std::filesystem::path coreAssemblyPath = GetExecutableDirectory() / "PixelEngineScripting.dll";
        PixelEngine::ScriptEngine::Init(coreAssemblyPath.string());
        PixelEngine::ScriptEngine::SetActiveScene(m_Context.ActiveScene);

        // Load project gameplay assembly
        std::string assemblyPath = "";
        if (std::filesystem::exists(projFilePath)) {
            std::ifstream fin(projFilePath);
            if (fin.is_open()) {
                try {
                    nlohmann::json projJson;
                    fin >> projJson;
                    assemblyPath = projJson.value("AssemblyPath", "");
                } catch (...) {}
                fin.close();
            }
        }
        if (!assemblyPath.empty() && std::filesystem::exists(projectPath / assemblyPath)) {
            PixelEngine::ScriptEngine::LoadGameAssembly((projectPath / assemblyPath).string());
        }

        // Initialize script components for all entities in the scene
        auto scriptView = m_Context.ActiveScene->Reg().view<PixelEngine::IDComponent>();
        for (auto entityID : scriptView) {
            PixelEngine::Entity entity = { entityID, m_Context.ActiveScene.get() };
            if (entity.HasComponent<PixelEngine::ScriptComponent>()) {
                PixelEngine::ScriptEngine::OnCreateEntity(entity);
            }
        }

        m_Context.ProjectLoaded = true;
        
        // Add to recent projects
        AddToRecentProjects(projectPathStr);
    }

    void CreateNewProject(const std::string& parentFolder, const std::string& name) {
        std::filesystem::path projectPath = std::filesystem::path(parentFolder) / name;
        std::filesystem::create_directories(projectPath);
        std::filesystem::create_directories(projectPath / "assets");
        std::filesystem::create_directories(projectPath / "assets" / "scenes");
        std::filesystem::create_directories(projectPath / "shaders");
        std::filesystem::create_directories(projectPath / "lib");
        std::filesystem::create_directories(projectPath / "src");

        // Copy default shaders if they exist in engine root
        std::filesystem::path defaultShaders = std::filesystem::absolute("shaders");
        if (std::filesystem::exists(defaultShaders)) {
            for (const auto& entry : std::filesystem::directory_iterator(defaultShaders)) {
                if (entry.is_regular_file()) {
                    std::filesystem::copy_file(entry.path(), projectPath / "shaders" / entry.path().filename(), std::filesystem::copy_options::overwrite_existing);
                }
            }
        }

        // Copy default test texture
        std::filesystem::path defaultTestPng = std::filesystem::absolute("assets/test.png");
        if (std::filesystem::exists(defaultTestPng)) {
            std::filesystem::copy_file(defaultTestPng, projectPath / "assets/test.png", std::filesystem::copy_options::overwrite_existing);
        }

        // Copy PixelEngineScripting.dll to project lib/
        std::filesystem::path scriptingDllSource = GetExecutableDirectory() / "PixelEngineScripting.dll";
        if (std::filesystem::exists(scriptingDllSource)) {
            std::filesystem::copy_file(scriptingDllSource, projectPath / "lib/PixelEngineScripting.dll", std::filesystem::copy_options::overwrite_existing);
        }

        // Generate UserGame.csproj
        std::ofstream csprojOut(projectPath / "UserGame.csproj");
        if (csprojOut.is_open()) {
            csprojOut << R"(<Project Sdk="Microsoft.NET.Sdk">
 
  <PropertyGroup>
    <TargetFramework>net8.0</TargetFramework>
    <ImplicitUsings>enable</ImplicitUsings>
    <Nullable>enable</Nullable>
    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>
    <OutputPath>bin</OutputPath>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
  </PropertyGroup>
 
  <ItemGroup>
    <Reference Include="PixelEngineScripting">
      <HintPath>lib\PixelEngineScripting.dll</HintPath>
    </Reference>
  </ItemGroup>
 
</Project>
)";
            csprojOut.close();
        }

        // Generate default Player.cs script template
        std::ofstream scriptOut(projectPath / "src/Player.cs");
        if (scriptOut.is_open()) {
            scriptOut << R"(using PixelEngine;
 
public class Player : MonoBehaviour {
    public override void OnCreate() {
        Log.Info("Player script OnCreate called!");
    }
 
    public override void OnUpdate(float dt) {
        var transform = GetComponent<TransformComponent>();
        var translation = transform.Translation;
        translation.X += 1.0f * dt;
        transform.Translation = translation;
    }
}
)";
            scriptOut.close();
        }

        // Create default empty scene
        std::shared_ptr<PixelEngine::Scene> tempScene = std::make_shared<PixelEngine::Scene>();
        
        // Setup initial default camera & sprite entity
        auto camera = tempScene->CreateEntity("Main Camera");
        
        PixelEngine::SceneSerializer serializer(*tempScene);
        serializer.Serialize((projectPath / "assets/scenes/startup.json").string());

        // Create .pixelproj configuration
        nlohmann::json projJson = {
            {"ProjectName", name},
            {"EngineVersion", "0.1.0"},
            {"StartupScene", "assets/scenes/startup.json"},
            {"AssemblyPath", "bin/UserGame.dll"}
        };
        std::string projFilePath = (projectPath / (name + ".pixelproj")).string();
        std::ofstream fout(projFilePath);
        if (fout.is_open()) {
            fout << projJson.dump(4) << std::endl;
            fout.close();
        }

        // Build the created C# project automatically
        std::string buildCmd = "dotnet build \"" + (projectPath / "UserGame.csproj").string() + "\" -c Debug";
        std::system(buildCmd.c_str());

        LoadProject(projectPath.string());
    }

    void LoadRecentProjects() {
        m_Context.RecentProjects.clear();
        std::string configPath = "recent_projects.json";
        if (std::filesystem::exists(configPath)) {
            std::ifstream fin(configPath);
            if (fin.is_open()) {
                try {
                    nlohmann::json configJson;
                    fin >> configJson;
                    if (configJson.is_array()) {
                        for (const auto& item : configJson) {
                            m_Context.RecentProjects.push_back(item.get<std::string>());
                        }
                    }
                } catch (...) {}
                fin.close();
            }
        }
    }

    void AddToRecentProjects(const std::string& path) {
        m_Context.RecentProjects.erase(std::remove(m_Context.RecentProjects.begin(), m_Context.RecentProjects.end(), path), m_Context.RecentProjects.end());
        m_Context.RecentProjects.insert(m_Context.RecentProjects.begin(), path);
        if (m_Context.RecentProjects.size() > 5) {
            m_Context.RecentProjects.resize(5);
        }
        std::string configPath = "recent_projects.json";
        std::ofstream fout(configPath);
        if (fout.is_open()) {
            nlohmann::json configJson = m_Context.RecentProjects;
            fout << configJson.dump(4) << std::endl;
            fout.close();
        }
    }

    PixelEngine::Entity CloneEntityOnly(PixelEngine::Entity source, PixelEngine::UUID newUUID, const std::string& nameSuffix = " (Copy)") {
        std::string newName = "Entity";
        if (source.HasComponent<PixelEngine::TagComponent>()) {
            newName = source.GetComponent<PixelEngine::TagComponent>().Tag + nameSuffix;
        }
        
        PixelEngine::Entity dest = m_Context.ActiveScene->CreateEntityWithUUID(newUUID, newName);
        
        if (source.HasComponent<PixelEngine::TransformComponent>()) {
            dest.GetComponent<PixelEngine::TransformComponent>() = source.GetComponent<PixelEngine::TransformComponent>();
        }
        if (source.HasComponent<PixelEngine::SpriteRendererComponent>()) {
            dest.AddComponent<PixelEngine::SpriteRendererComponent>(source.GetComponent<PixelEngine::SpriteRendererComponent>());
        }
        if (source.HasComponent<PixelEngine::MeshRendererComponent>()) {
            dest.AddComponent<PixelEngine::MeshRendererComponent>(source.GetComponent<PixelEngine::MeshRendererComponent>());
        }
        if (source.HasComponent<PixelEngine::VelocityComponent>()) {
            dest.AddComponent<PixelEngine::VelocityComponent>(source.GetComponent<PixelEngine::VelocityComponent>());
        }
        if (source.HasComponent<PixelEngine::SpriteAnimationComponent>()) {
            dest.AddComponent<PixelEngine::SpriteAnimationComponent>(source.GetComponent<PixelEngine::SpriteAnimationComponent>());
        }
        if (source.HasComponent<PixelEngine::TilemapComponent>()) {
            dest.AddComponent<PixelEngine::TilemapComponent>(source.GetComponent<PixelEngine::TilemapComponent>());
        }
        if (source.HasComponent<PixelEngine::AnimatorComponent>()) {
            dest.AddComponent<PixelEngine::AnimatorComponent>(source.GetComponent<PixelEngine::AnimatorComponent>());
        }
        if (source.HasComponent<PixelEngine::AudioSourceComponent>()) {
            dest.AddComponent<PixelEngine::AudioSourceComponent>(source.GetComponent<PixelEngine::AudioSourceComponent>());
        }
        if (source.HasComponent<PixelEngine::PrefabComponent>()) {
            dest.AddComponent<PixelEngine::PrefabComponent>(source.GetComponent<PixelEngine::PrefabComponent>());
        }
        if (source.HasComponent<PixelEngine::ScriptComponent>()) {
            dest.AddComponent<PixelEngine::ScriptComponent>(source.GetComponent<PixelEngine::ScriptComponent>());
        }
        
        return dest;
    }

    PixelEngine::Entity DuplicateSubtree(PixelEngine::Entity source, PixelEngine::UUID parentUUID, const std::string& suffix) {
        PixelEngine::UUID newUUID;
        PixelEngine::Entity dest = CloneEntityOnly(source, newUUID, suffix);
        
        if (source.HasComponent<PixelEngine::HierarchyComponent>() || parentUUID != 0) {
            dest.AddComponent<PixelEngine::HierarchyComponent>();
            auto& destHc = dest.GetComponent<PixelEngine::HierarchyComponent>();
            
            if (parentUUID != 0) {
                destHc.Parent = parentUUID;
                auto parentEnt = m_Context.ActiveScene->GetEntityByUUID(parentUUID);
                if (parentEnt && parentEnt.HasComponent<PixelEngine::HierarchyComponent>()) {
                    parentEnt.GetComponent<PixelEngine::HierarchyComponent>().Children.push_back(newUUID);
                }
            } else {
                PixelEngine::UUID origParentUUID = source.GetComponent<PixelEngine::HierarchyComponent>().Parent;
                destHc.Parent = origParentUUID;
                if (origParentUUID != 0) {
                    auto parentEnt = m_Context.ActiveScene->GetEntityByUUID(origParentUUID);
                    if (parentEnt && parentEnt.HasComponent<PixelEngine::HierarchyComponent>()) {
                        parentEnt.GetComponent<PixelEngine::HierarchyComponent>().Children.push_back(newUUID);
                    }
                }
            }
        }
        
        if (source.HasComponent<PixelEngine::HierarchyComponent>()) {
            auto& srcHc = source.GetComponent<PixelEngine::HierarchyComponent>();
            for (auto childUUID : srcHc.Children) {
                auto childEntity = m_Context.ActiveScene->GetEntityByUUID(childUUID);
                if (childEntity) {
                    DuplicateSubtree(childEntity, newUUID, "");
                }
            }
        }
        
        return dest;
    }

    void DeleteEntity(PixelEngine::Entity entity) {
        if (!entity) return;
        PixelEngine::UUID entityUUID = entity.GetComponent<PixelEngine::IDComponent>().ID;

        if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
            auto& hc = entity.GetComponent<PixelEngine::HierarchyComponent>();
            std::vector<PixelEngine::UUID> childrenCopy = hc.Children;
            for (auto childUUID : childrenCopy) {
                auto childEntity = m_Context.ActiveScene->GetEntityByUUID(childUUID);
                if (childEntity) {
                    DeleteEntity(childEntity);
                }
            }
        }

        if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
            auto& hc = entity.GetComponent<PixelEngine::HierarchyComponent>();
            if (hc.Parent != 0) {
                auto parentEntity = m_Context.ActiveScene->GetEntityByUUID(hc.Parent);
                if (parentEntity && parentEntity.HasComponent<PixelEngine::HierarchyComponent>()) {
                    auto& parentHc = parentEntity.GetComponent<PixelEngine::HierarchyComponent>();
                    parentHc.Children.erase(
                        std::remove(parentHc.Children.begin(), parentHc.Children.end(), entityUUID),
                        parentHc.Children.end()
                    );
                }
            }
        }

        m_Context.ActiveScene->DestroyEntity(entity);
    }

    void GroupEntity(PixelEngine::Entity entity) {
        if (!entity) return;
        PixelEngine::UUID entityUUID = entity.GetComponent<PixelEngine::IDComponent>().ID;

        PixelEngine::Entity newParent = m_Context.ActiveScene->CreateEntity("Grouped Entity");
        newParent.AddComponent<PixelEngine::HierarchyComponent>();
        auto& parentHc = newParent.GetComponent<PixelEngine::HierarchyComponent>();
        parentHc.Children.push_back(entityUUID);

        PixelEngine::UUID originalParentUUID = 0;
        if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
            auto& entityHc = entity.GetComponent<PixelEngine::HierarchyComponent>();
            originalParentUUID = entityHc.Parent;
        } else {
            entity.AddComponent<PixelEngine::HierarchyComponent>();
        }

        auto& entityHc = entity.GetComponent<PixelEngine::HierarchyComponent>();
        entityHc.Parent = newParent.GetComponent<PixelEngine::IDComponent>().ID;

        if (originalParentUUID != 0) {
            parentHc.Parent = originalParentUUID;
            auto grandparentEnt = m_Context.ActiveScene->GetEntityByUUID(originalParentUUID);
            if (grandparentEnt && grandparentEnt.HasComponent<PixelEngine::HierarchyComponent>()) {
                auto& grandparentHc = grandparentEnt.GetComponent<PixelEngine::HierarchyComponent>();
                auto it = std::find(grandparentHc.Children.begin(), grandparentHc.Children.end(), entityUUID);
                if (it != grandparentHc.Children.end()) {
                    *it = newParent.GetComponent<PixelEngine::IDComponent>().ID;
                } else {
                    grandparentHc.Children.push_back(newParent.GetComponent<PixelEngine::IDComponent>().ID);
                }
            }
        }
    }

    PixelEngine::Entity CreatePresetEntity(const std::string& presetName, PixelEngine::UUID parentUUID = 0) {
        PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
        nlohmann::json beforeState = serializer.SerializeToJson();

        std::string name = "New Entity";
        if (presetName == "Empty Entity") name = "Empty Entity";
        else if (presetName == "Sprite Renderer") name = "Sprite Entity";
        else if (presetName == "Camera") name = "Main Camera";
        else if (presetName == "Audio Source") name = "Audio Source";
        else if (presetName == "Animator") name = "Animator Entity";
        else if (presetName == "Tilemap") name = "Tilemap Entity";

        PixelEngine::Entity ent = m_Context.ActiveScene->CreateEntity(name);
        
        if (presetName == "Sprite Renderer") {
            ent.AddComponent<PixelEngine::SpriteRendererComponent>();
        } else if (presetName == "Audio Source") {
            ent.AddComponent<PixelEngine::AudioSourceComponent>();
        } else if (presetName == "Animator") {
            ent.AddComponent<PixelEngine::SpriteRendererComponent>();
            ent.AddComponent<PixelEngine::AnimatorComponent>();
        } else if (presetName == "Tilemap") {
            ent.AddComponent<PixelEngine::TilemapComponent>();
        }

        if (ent.HasComponent<PixelEngine::TransformComponent>()) {
            auto& tc = ent.GetComponent<PixelEngine::TransformComponent>();
            tc.Translation = { 0.0f, 0.0f, 0.0f };
            tc.Rotation = { 0.0f, 0.0f, 0.0f };
            tc.Scale = { 1.0f, 1.0f, 1.0f };
        }

        if (parentUUID != 0) {
            ent.AddComponent<PixelEngine::HierarchyComponent>(parentUUID);
            auto parentEnt = m_Context.ActiveScene->GetEntityByUUID(parentUUID);
            if (parentEnt) {
                if (!parentEnt.HasComponent<PixelEngine::HierarchyComponent>()) {
                    parentEnt.AddComponent<PixelEngine::HierarchyComponent>();
                }
                parentEnt.GetComponent<PixelEngine::HierarchyComponent>().Children.push_back(ent.GetComponent<PixelEngine::IDComponent>().ID);
            }
        }

        m_Context.SelectedEntity = ent;

        nlohmann::json afterState = serializer.SerializeToJson();
        PixelEngine::EditorHistory::PushCommand(
            std::make_unique<PixelEngine::SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Create " + presetName)
        );

        return ent;
    }

    void ReorderEntity(PixelEngine::Entity entity, bool moveUp) {
        if (!entity || !entity.HasComponent<PixelEngine::HierarchyComponent>()) return;
        auto& hc = entity.GetComponent<PixelEngine::HierarchyComponent>();
        if (hc.Parent == 0) return;

        auto parentEnt = m_Context.ActiveScene->GetEntityByUUID(hc.Parent);
        if (!parentEnt || !parentEnt.HasComponent<PixelEngine::HierarchyComponent>()) return;

        auto& parentHc = parentEnt.GetComponent<PixelEngine::HierarchyComponent>();
        auto& children = parentHc.Children;
        PixelEngine::UUID myUUID = entity.GetComponent<PixelEngine::IDComponent>().ID;

        auto it = std::find(children.begin(), children.end(), myUUID);
        if (it != children.end()) {
            size_t index = std::distance(children.begin(), it);
            if (moveUp) {
                if (index > 0) {
                    std::swap(children[index], children[index - 1]);
                }
            } else {
                if (index < children.size() - 1) {
                    std::swap(children[index], children[index + 1]);
                }
            }
        }
    }

    void CompileCSProjectAsync() {
        if (!m_Context.ProjectLoaded) return;
        
        m_Context.AssemblyReloadPending = false;
        std::string projectPathStr = m_Context.ProjectPath;
        
        std::thread buildThread([this, projectPathStr]() {
            PX_CORE_INFO("Starting background C# script compilation...");
            std::filesystem::path projectPath(projectPathStr);
            std::string buildCmd = "dotnet build \"" + (projectPath / "UserGame.csproj").string() + "\" -c Debug > dotnet_build.log 2>&1";
            int result = std::system(buildCmd.c_str());
            if (result == 0) {
                PX_CORE_INFO("C# Script compilation completed successfully.");
                m_Context.AssemblyReloadPending = true;
            } else {
                PX_CORE_ERROR("C# Script compilation failed! See dotnet_build.log for details.");
            }
        });
        buildThread.detach();
    }

    void ApplyPrefabOverrides(PixelEngine::Entity instanceRoot, PixelEngine::UUID prefabID) {
        std::filesystem::path prefabPath = PixelEngine::FindPrefabPath(prefabID, m_Context.ProjectLoaded, m_Context.ProjectPath);
        if (prefabPath.empty()) {
            PX_CORE_ERROR("Failed to find prefab file on disk for PrefabID {0}", (uint64_t)prefabID);
            return;
        }

        std::vector<PixelEngine::Entity> instanceEntities;
        std::function<void(PixelEngine::Entity)> collectInstanceEntities = [&](PixelEngine::Entity ent) {
            instanceEntities.push_back(ent);
            if (ent.HasComponent<PixelEngine::HierarchyComponent>()) {
                auto& hc = ent.GetComponent<PixelEngine::HierarchyComponent>();
                for (auto childUUID : hc.Children) {
                    auto childEnt = m_Context.ActiveScene->GetEntityByUUID(childUUID);
                    if (childEnt) {
                        collectInstanceEntities(childEnt);
                    }
                }
            }
        };
        collectInstanceEntities(instanceRoot);

        std::unordered_map<uint64_t, uint64_t> sceneToPrefabUUIDMap;
        for (auto ent : instanceEntities) {
            auto& pc = ent.HasComponent<PixelEngine::PrefabComponent>() ? ent.GetComponent<PixelEngine::PrefabComponent>() : ent.AddComponent<PixelEngine::PrefabComponent>(prefabID);
            pc.PrefabID = prefabID;
            if (pc.OriginalUUID == 0) {
                pc.OriginalUUID = PixelEngine::UUID();
            }
            uint64_t sceneUUID = static_cast<uint64_t>(ent.GetComponent<PixelEngine::IDComponent>().ID);
            sceneToPrefabUUIDMap[sceneUUID] = static_cast<uint64_t>(pc.OriginalUUID);
        }

        PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
        nlohmann::json sceneJson = serializer.SerializeToJson();

        nlohmann::json entitiesArray = nlohmann::json::array();
        for (auto& entityJson : sceneJson["Entities"]) {
            uint64_t sceneUUID = entityJson["UUID"].get<uint64_t>();
            if (sceneToPrefabUUIDMap.count(sceneUUID) > 0) {
                uint64_t prefabUUID = sceneToPrefabUUIDMap[sceneUUID];
                entityJson["UUID"] = prefabUUID;

                if (entityJson.contains("HierarchyComponent")) {
                    auto& hcJson = entityJson["HierarchyComponent"];
                    uint64_t oldParent = hcJson["Parent"].get<uint64_t>();
                    if (oldParent != 0 && sceneToPrefabUUIDMap.count(oldParent) > 0) {
                        hcJson["Parent"] = sceneToPrefabUUIDMap[oldParent];
                    } else {
                        hcJson["Parent"] = 0ull;
                    }
                    nlohmann::json newChildrenJson = nlohmann::json::array();
                    for (auto& childVal : hcJson["Children"]) {
                        uint64_t oldChild = childVal.get<uint64_t>();
                        if (sceneToPrefabUUIDMap.count(oldChild) > 0) {
                            newChildrenJson.push_back(sceneToPrefabUUIDMap[oldChild]);
                        }
                    }
                    hcJson["Children"] = newChildrenJson;
                }

                if (entityJson.contains("PrefabComponent")) {
                    entityJson["PrefabComponent"]["OverriddenFields"] = nlohmann::json::array();
                    entityJson["PrefabComponent"]["OriginalUUID"] = prefabUUID;
                }

                entitiesArray.push_back(entityJson);
            }
        }

        nlohmann::json prefabJson;
        prefabJson["PrefabID"] = static_cast<uint64_t>(prefabID);
        prefabJson["Entities"] = entitiesArray;

        std::ofstream fout(prefabPath);
        if (fout.is_open()) {
            fout << std::setw(4) << prefabJson << std::endl;
            PX_CORE_INFO("Applied overrides and updated prefab asset on disk: {0}", prefabPath.string());
        } else {
            PX_CORE_ERROR("Failed to write updated prefab to {0}", prefabPath.string());
            return;
        }

        for (auto ent : instanceEntities) {
            if (ent.HasComponent<PixelEngine::PrefabComponent>()) {
                ent.GetComponent<PixelEngine::PrefabComponent>().OverriddenFields.clear();
            }
        }

        ReconcileAllPrefabInstances(prefabID);
    }

    void RevertPrefabOverrides(PixelEngine::Entity instanceRoot, PixelEngine::UUID prefabID) {
        std::vector<PixelEngine::Entity> subtree;
        std::function<void(PixelEngine::Entity)> collectSubtree = [&](PixelEngine::Entity ent) {
            subtree.push_back(ent);
            if (ent.HasComponent<PixelEngine::HierarchyComponent>()) {
                auto& hc = ent.GetComponent<PixelEngine::HierarchyComponent>();
                for (auto childUUID : hc.Children) {
                    auto childEnt = m_Context.ActiveScene->GetEntityByUUID(childUUID);
                    if (childEnt) {
                        collectSubtree(childEnt);
                    }
                }
            }
        };
        collectSubtree(instanceRoot);

        for (auto ent : subtree) {
            if (ent.HasComponent<PixelEngine::PrefabComponent>()) {
                ent.GetComponent<PixelEngine::PrefabComponent>().OverriddenFields.clear();
            }
        }

        ReconcilePrefabSubtree(instanceRoot, prefabID);
    }

    static glm::vec3 DeserializeVec3(const nlohmann::json& j) {
        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }

    static glm::vec4 DeserializeVec4(const nlohmann::json& j) {
        return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    }

    void ReconcileEntityProperties(PixelEngine::Entity sceneEnt, const nlohmann::json& prefabEntJson) {
        if (!sceneEnt) return;

        auto hasOverride = [&](const std::string& field) {
            if (!sceneEnt.HasComponent<PixelEngine::PrefabComponent>()) return false;
            auto& pc = sceneEnt.GetComponent<PixelEngine::PrefabComponent>();
            return std::find(pc.OverriddenFields.begin(), pc.OverriddenFields.end(), field) != pc.OverriddenFields.end();
        };

        if (prefabEntJson.contains("Tag")) {
            if (!hasOverride("TagComponent.Tag")) {
                sceneEnt.GetComponent<PixelEngine::TagComponent>().Tag = prefabEntJson["Tag"].get<std::string>();
            }
        }

        if (prefabEntJson.contains("TransformComponent")) {
            auto& tcJson = prefabEntJson["TransformComponent"];
            auto& tc = sceneEnt.GetComponent<PixelEngine::TransformComponent>();
            if (!hasOverride("TransformComponent.Translation")) {
                tc.Translation = DeserializeVec3(tcJson["Translation"]);
            }
            if (!hasOverride("TransformComponent.Rotation")) {
                tc.Rotation = DeserializeVec3(tcJson["Rotation"]);
            }
            if (!hasOverride("TransformComponent.Scale")) {
                tc.Scale = DeserializeVec3(tcJson["Scale"]);
            }
        }

        if (prefabEntJson.contains("SpriteRendererComponent")) {
            auto& scJson = prefabEntJson["SpriteRendererComponent"];
            auto& matJson = scJson["Material"];
            if (!sceneEnt.HasComponent<PixelEngine::SpriteRendererComponent>()) {
                sceneEnt.AddComponent<PixelEngine::SpriteRendererComponent>();
            }
            auto& sc = sceneEnt.GetComponent<PixelEngine::SpriteRendererComponent>();
            if (!hasOverride("SpriteRendererComponent.Color")) {
                sc.Mat.Color = DeserializeVec4(matJson["Color"]);
            }
            if (!hasOverride("SpriteRendererComponent.Blend")) {
                sc.Mat.Blend = static_cast<PixelEngine::BlendMode>(matJson.value("Blend", 1));
            }
            if (!hasOverride("SpriteRendererComponent.TextureID")) {
                sc.Mat.TextureID = PixelEngine::UUID(matJson["TextureID"].get<uint64_t>());
            }
            if (!hasOverride("SpriteRendererComponent.ShaderName")) {
                sc.Mat.ShaderName = matJson.value("ShaderName", "sprite");
            }
        } else {
            if (sceneEnt.HasComponent<PixelEngine::SpriteRendererComponent>() && !hasOverride("SpriteRendererComponent")) {
                sceneEnt.RemoveComponent<PixelEngine::SpriteRendererComponent>();
            }
        }

        if (prefabEntJson.contains("MeshRendererComponent")) {
            auto& mcJson = prefabEntJson["MeshRendererComponent"];
            if (!sceneEnt.HasComponent<PixelEngine::MeshRendererComponent>()) {
                sceneEnt.AddComponent<PixelEngine::MeshRendererComponent>();
            }
            auto& mc = sceneEnt.GetComponent<PixelEngine::MeshRendererComponent>();
            if (!hasOverride("MeshRendererComponent.Color")) {
                mc.Color = DeserializeVec4(mcJson["Color"]);
            }
            if (!hasOverride("MeshRendererComponent.TextureID")) {
                mc.TextureID = PixelEngine::UUID(mcJson["TextureID"].get<uint64_t>());
            }
        } else {
            if (sceneEnt.HasComponent<PixelEngine::MeshRendererComponent>() && !hasOverride("MeshRendererComponent")) {
                sceneEnt.RemoveComponent<PixelEngine::MeshRendererComponent>();
            }
        }

        if (prefabEntJson.contains("VelocityComponent")) {
            auto& vcJson = prefabEntJson["VelocityComponent"];
            if (!sceneEnt.HasComponent<PixelEngine::VelocityComponent>()) {
                sceneEnt.AddComponent<PixelEngine::VelocityComponent>();
            }
            auto& vc = sceneEnt.GetComponent<PixelEngine::VelocityComponent>();
            if (!hasOverride("VelocityComponent.Linear")) {
                vc.Linear = DeserializeVec3(vcJson["Linear"]);
            }
            if (!hasOverride("VelocityComponent.Angular")) {
                vc.Angular = DeserializeVec3(vcJson["Angular"]);
            }
        } else {
            if (sceneEnt.HasComponent<PixelEngine::VelocityComponent>() && !hasOverride("VelocityComponent")) {
                sceneEnt.RemoveComponent<PixelEngine::VelocityComponent>();
            }
        }

        if (prefabEntJson.contains("SpriteAnimationComponent")) {
            auto& acJson = prefabEntJson["SpriteAnimationComponent"];
            if (!sceneEnt.HasComponent<PixelEngine::SpriteAnimationComponent>()) {
                sceneEnt.AddComponent<PixelEngine::SpriteAnimationComponent>();
            }
            auto& ac = sceneEnt.GetComponent<PixelEngine::SpriteAnimationComponent>();
            if (!hasOverride("SpriteAnimationComponent.FrameTime")) {
                ac.FrameTime = acJson.value("FrameTime", 0.1f);
            }
            if (!hasOverride("SpriteAnimationComponent.Loop")) {
                ac.Loop = acJson.value("Loop", true);
            }
            if (!hasOverride("SpriteAnimationComponent.Playing")) {
                ac.Playing = acJson.value("Playing", true);
            }
            if (!hasOverride("SpriteAnimationComponent.Textures")) {
                ac.Textures.clear();
                for (auto& frameVal : acJson["Textures"]) {
                    ac.Textures.push_back(PixelEngine::UUID(frameVal.get<uint64_t>()));
                }
            }
        } else {
            if (sceneEnt.HasComponent<PixelEngine::SpriteAnimationComponent>() && !hasOverride("SpriteAnimationComponent")) {
                sceneEnt.RemoveComponent<PixelEngine::SpriteAnimationComponent>();
            }
        }

        if (prefabEntJson.contains("TilemapComponent")) {
            auto& tmJson = prefabEntJson["TilemapComponent"];
            if (!sceneEnt.HasComponent<PixelEngine::TilemapComponent>()) {
                sceneEnt.AddComponent<PixelEngine::TilemapComponent>();
            }
            auto& tc = sceneEnt.GetComponent<PixelEngine::TilemapComponent>();
            if (!hasOverride("TilemapComponent.TilesetID")) {
                tc.TilesetID = PixelEngine::UUID(tmJson.value("TilesetID", 0ull));
            }
            if (!hasOverride("TilemapComponent.TileSize")) {
                tc.TileSize = tmJson.value("TileSize", 16u);
            }
            if (!hasOverride("TilemapComponent.RenderLayer")) {
                tc.RenderLayer = tmJson.value("RenderLayer", 0);
            }
            if (!hasOverride("TilemapComponent.Chunks")) {
                tc.Chunks.clear();
                if (tmJson.contains("Chunks") && tmJson["Chunks"].is_array()) {
                    for (auto& chunkJson : tmJson["Chunks"]) {
                        int cx = chunkJson.value("x", 0);
                        int cy = chunkJson.value("y", 0);
                        PixelEngine::TilemapChunk chunk;
                        if (chunkJson.contains("Tiles") && chunkJson["Tiles"].is_array()) {
                            int idx = 0;
                            for (auto& tileVal : chunkJson["Tiles"]) {
                                if (idx < PixelEngine::TilemapChunk::ChunkSize * PixelEngine::TilemapChunk::ChunkSize) {
                                    chunk.Tiles[idx].TileIndex = tileVal.get<uint32_t>();
                                    idx++;
                                }
                            }
                        }
                        tc.Chunks[{cx, cy}] = chunk;
                    }
                }
            }
        } else {
            if (sceneEnt.HasComponent<PixelEngine::TilemapComponent>() && !hasOverride("TilemapComponent")) {
                sceneEnt.RemoveComponent<PixelEngine::TilemapComponent>();
            }
        }

        if (prefabEntJson.contains("AnimatorComponent")) {
            auto& acJson = prefabEntJson["AnimatorComponent"];
            if (!sceneEnt.HasComponent<PixelEngine::AnimatorComponent>()) {
                sceneEnt.AddComponent<PixelEngine::AnimatorComponent>();
            }
            auto& ac = sceneEnt.GetComponent<PixelEngine::AnimatorComponent>();
            if (!hasOverride("AnimatorComponent.SpriteSheetID")) {
                ac.SpriteSheetID = PixelEngine::UUID(acJson.value("SpriteSheetID", 0ull));
            }
            if (!hasOverride("AnimatorComponent.Playing")) {
                ac.Playing = acJson.value("Playing", true);
            }
            if (!hasOverride("AnimatorComponent.CurrentClip")) {
                ac.CurrentClip = acJson.value("CurrentClip", "");
            }
            if (!hasOverride("AnimatorComponent.CurrentFrame")) {
                ac.CurrentFrame = acJson.value("CurrentFrame", 0);
            }
            if (!hasOverride("AnimatorComponent.Clips")) {
                ac.Clips.clear();
                if (acJson.contains("Clips") && acJson["Clips"].is_array()) {
                    for (auto& clipJson : acJson["Clips"]) {
                        PixelEngine::AnimationClip clip;
                        clip.Name = clipJson.value("Name", "");
                        clip.FPS = clipJson.value("FPS", 10.0f);
                        clip.Loop = clipJson.value("Loop", true);
                        if (clipJson.contains("Frames") && clipJson["Frames"].is_array()) {
                            for (auto& frameJson : clipJson["Frames"]) {
                                PixelEngine::AnimationFrame frame;
                                frame.FrameName = frameJson.value("FrameName", "");
                                frame.EventName = frameJson.value("EventName", "");
                                clip.Frames.push_back(frame);
                            }
                        }
                        ac.Clips.push_back(clip);
                    }
                }
            }
        } else {
            if (sceneEnt.HasComponent<PixelEngine::AnimatorComponent>() && !hasOverride("AnimatorComponent")) {
                sceneEnt.RemoveComponent<PixelEngine::AnimatorComponent>();
            }
        }

        if (prefabEntJson.contains("AudioSourceComponent")) {
            auto& audioJson = prefabEntJson["AudioSourceComponent"];
            if (!sceneEnt.HasComponent<PixelEngine::AudioSourceComponent>()) {
                sceneEnt.AddComponent<PixelEngine::AudioSourceComponent>();
            }
            auto& asc = sceneEnt.GetComponent<PixelEngine::AudioSourceComponent>();
            if (!hasOverride("AudioSourceComponent.ClipID")) {
                asc.ClipID = PixelEngine::UUID(audioJson.value("ClipID", 0ull));
            }
            if (!hasOverride("AudioSourceComponent.Loop")) {
                asc.Loop = audioJson.value("Loop", false);
            }
            if (!hasOverride("AudioSourceComponent.PlayOnStart")) {
                asc.PlayOnStart = audioJson.value("PlayOnStart", false);
            }
            if (!hasOverride("AudioSourceComponent.Volume")) {
                asc.Volume = audioJson.value("Volume", 1.0f);
            }
            if (!hasOverride("AudioSourceComponent.IsMusic")) {
                asc.IsMusic = audioJson.value("IsMusic", false);
            }
        } else {
            if (sceneEnt.HasComponent<PixelEngine::AudioSourceComponent>() && !hasOverride("AudioSourceComponent")) {
                sceneEnt.RemoveComponent<PixelEngine::AudioSourceComponent>();
            }
        }
    }

    void ReconcilePrefabSubtree(PixelEngine::Entity instanceRoot, PixelEngine::UUID prefabID) {
        std::filesystem::path prefabPath = PixelEngine::FindPrefabPath(prefabID, m_Context.ProjectLoaded, m_Context.ProjectPath);
        if (prefabPath.empty()) return;

        std::ifstream fin(prefabPath);
        if (!fin.is_open()) return;
        nlohmann::json prefabJson;
        try {
            fin >> prefabJson;
        } catch (...) {
            return;
        }

        if (!prefabJson.contains("Entities") || !prefabJson["Entities"].is_array()) return;

        std::vector<PixelEngine::Entity> subtree;
        std::function<void(PixelEngine::Entity)> collectSubtree = [&](PixelEngine::Entity ent) {
            subtree.push_back(ent);
            if (ent.HasComponent<PixelEngine::HierarchyComponent>()) {
                auto& hc = ent.GetComponent<PixelEngine::HierarchyComponent>();
                for (auto childUUID : hc.Children) {
                    auto childEnt = m_Context.ActiveScene->GetEntityByUUID(childUUID);
                    if (childEnt) {
                        collectSubtree(childEnt);
                    }
                }
            }
        };
        collectSubtree(instanceRoot);

        for (const auto& entityJson : prefabJson["Entities"]) {
            uint64_t prefabUUID = entityJson["UUID"].get<uint64_t>();
            PixelEngine::Entity targetEnt;
            for (auto ent : subtree) {
                if (ent.HasComponent<PixelEngine::PrefabComponent>()) {
                    if (static_cast<uint64_t>(ent.GetComponent<PixelEngine::PrefabComponent>().OriginalUUID) == prefabUUID) {
                        targetEnt = ent;
                        break;
                    }
                }
            }

            if (targetEnt) {
                ReconcileEntityProperties(targetEnt, entityJson);
            }
        }
    }

    void ReconcileAllPrefabInstances(PixelEngine::UUID prefabID) {
        auto view = m_Context.ActiveScene->Reg().view<PixelEngine::PrefabComponent>();
        std::vector<PixelEngine::Entity> roots;

        for (auto entityID : view) {
            PixelEngine::Entity entity = { entityID, m_Context.ActiveScene.get() };
            if (entity.GetComponent<PixelEngine::PrefabComponent>().PrefabID == prefabID) {
                bool isRootInstance = true;
                if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
                    auto parentUUID = entity.GetComponent<PixelEngine::HierarchyComponent>().Parent;
                    if (parentUUID != 0) {
                        auto parentEnt = m_Context.ActiveScene->GetEntityByUUID(parentUUID);
                        if (parentEnt && parentEnt.HasComponent<PixelEngine::PrefabComponent>() && parentEnt.GetComponent<PixelEngine::PrefabComponent>().PrefabID == prefabID) {
                            isRootInstance = false;
                        }
                    }
                }
                if (isRootInstance) {
                    roots.push_back(entity);
                }
            }
        }

        for (auto root : roots) {
            ReconcilePrefabSubtree(root, prefabID);
        }
    }

    void SaveEntityAsPrefab(PixelEngine::UUID entityUUID, const std::filesystem::path& folderPath) {
        auto entity = m_Context.ActiveScene->GetEntityByUUID(entityUUID);
        if (!entity) return;

        auto& tag = entity.GetComponent<PixelEngine::TagComponent>().Tag;
        std::string filename = tag + ".prefab.json";
        std::filesystem::path prefabPath = folderPath / filename;

        PixelEngine::UUID prefabID;

        if (!entity.HasComponent<PixelEngine::PrefabComponent>()) {
            entity.AddComponent<PixelEngine::PrefabComponent>(prefabID, entityUUID);
        } else {
            entity.GetComponent<PixelEngine::PrefabComponent>().PrefabID = prefabID;
            entity.GetComponent<PixelEngine::PrefabComponent>().OriginalUUID = entityUUID;
        }

        std::unordered_set<uint64_t> subtreeUUIDs;
        std::function<void(PixelEngine::Entity)> collectAndTagDescendants = [&](PixelEngine::Entity ent) {
            uint64_t entUUID = static_cast<uint64_t>(ent.GetComponent<PixelEngine::IDComponent>().ID);
            subtreeUUIDs.insert(entUUID);
            
            if (!ent.HasComponent<PixelEngine::PrefabComponent>()) {
                ent.AddComponent<PixelEngine::PrefabComponent>(prefabID, ent.GetComponent<PixelEngine::IDComponent>().ID);
            } else {
                ent.GetComponent<PixelEngine::PrefabComponent>().PrefabID = prefabID;
                if (ent.GetComponent<PixelEngine::PrefabComponent>().OriginalUUID == 0) {
                    ent.GetComponent<PixelEngine::PrefabComponent>().OriginalUUID = ent.GetComponent<PixelEngine::IDComponent>().ID;
                }
            }

            if (ent.HasComponent<PixelEngine::HierarchyComponent>()) {
                auto& hc = ent.GetComponent<PixelEngine::HierarchyComponent>();
                for (auto childUUID : hc.Children) {
                    auto childEnt = m_Context.ActiveScene->GetEntityByUUID(childUUID);
                    if (childEnt) {
                        collectAndTagDescendants(childEnt);
                    }
                }
            }
        };
        collectAndTagDescendants(entity);

        PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
        nlohmann::json sceneJson = serializer.SerializeToJson();

        nlohmann::json entitiesArray = nlohmann::json::array();
        for (auto& entityJson : sceneJson["Entities"]) {
            uint64_t uuidVal = entityJson["UUID"].get<uint64_t>();
            if (subtreeUUIDs.count(uuidVal) > 0) {
                if (uuidVal == static_cast<uint64_t>(entityUUID)) {
                    if (entityJson.contains("HierarchyComponent")) {
                        entityJson["HierarchyComponent"]["Parent"] = 0ull;
                    }
                }
                entitiesArray.push_back(entityJson);
            }
        }

        nlohmann::json prefabJson;
        prefabJson["PrefabID"] = static_cast<uint64_t>(prefabID);
        prefabJson["Entities"] = entitiesArray;

        std::ofstream fout(prefabPath);
        if (fout.is_open()) {
            fout << std::setw(4) << prefabJson << std::endl;
            PX_CORE_INFO("Saved prefab: {0}", prefabPath.string());
        } else {
            PX_CORE_ERROR("Failed to write prefab to {0}", prefabPath.string());
        }
    }

    void InstantiatePrefab(const std::string& prefabPath) {
        std::ifstream fin(prefabPath);
        if (!fin.is_open()) {
            PX_CORE_ERROR("Failed to open prefab file: {0}", prefabPath);
            return;
        }

        nlohmann::json prefabJson;
        try {
            fin >> prefabJson;
        } catch (const std::exception& e) {
            PX_CORE_ERROR("Failed to parse prefab JSON: {0}", e.what());
            return;
        }

        if (!prefabJson.contains("Entities") || !prefabJson["Entities"].is_array()) {
            PX_CORE_ERROR("Invalid prefab file format: {0}", prefabPath);
            return;
        }

        uint64_t prefabIDVal = prefabJson.value("PrefabID", 0ull);
        PixelEngine::UUID prefabID(prefabIDVal);

        PixelEngine::SceneSerializer serializer(*m_Context.ActiveScene);
        nlohmann::json beforeState = serializer.SerializeToJson();

        std::unordered_map<uint64_t, uint64_t> uuidMap;
        for (const auto& entityJson : prefabJson["Entities"]) {
            uint64_t oldUUIDVal = entityJson["UUID"].get<uint64_t>();
            PixelEngine::UUID newUUID;
            uuidMap[oldUUIDVal] = static_cast<uint64_t>(newUUID);
        }

        PixelEngine::Entity rootEntity;
        bool isFirst = true;

        for (const auto& entityJson : prefabJson["Entities"]) {
            uint64_t oldUUIDVal = entityJson["UUID"].get<uint64_t>();
            PixelEngine::UUID newUUID(uuidMap[oldUUIDVal]);

            std::string name = entityJson.value("Tag", "Prefab Entity");
            PixelEngine::Entity newEntity = m_Context.ActiveScene->CreateEntityWithUUID(newUUID, name);

            if (entityJson.find("TransformComponent") != entityJson.end()) {
                auto& tc = newEntity.GetComponent<PixelEngine::TransformComponent>();
                auto& tcJson = entityJson["TransformComponent"];
                tc.Translation = DeserializeVec3(tcJson["Translation"]);
                tc.Rotation = DeserializeVec3(tcJson["Rotation"]);
                tc.Scale = DeserializeVec3(tcJson["Scale"]);
            }

            if (entityJson.find("SpriteRendererComponent") != entityJson.end()) {
                auto& sc = newEntity.AddComponent<PixelEngine::SpriteRendererComponent>();
                auto& scJson = entityJson["SpriteRendererComponent"];
                auto& matJson = scJson["Material"];
                sc.Mat.ShaderName = matJson.value("ShaderName", "sprite");
                sc.Mat.TextureID = PixelEngine::UUID(matJson["TextureID"].get<uint64_t>());
                sc.Mat.Color = DeserializeVec4(matJson["Color"]);
                sc.Mat.Blend = static_cast<PixelEngine::BlendMode>(matJson.value("Blend", 1));
            }

            if (entityJson.find("MeshRendererComponent") != entityJson.end()) {
                auto& mc = newEntity.AddComponent<PixelEngine::MeshRendererComponent>();
                auto& mcJson = entityJson["MeshRendererComponent"];
                mc.Color = DeserializeVec4(mcJson["Color"]);
                mc.TextureID = PixelEngine::UUID(mcJson["TextureID"].get<uint64_t>());
            }

            if (entityJson.find("HierarchyComponent") != entityJson.end()) {
                auto& hc = newEntity.AddComponent<PixelEngine::HierarchyComponent>();
                auto& hcJson = entityJson["HierarchyComponent"];
                uint64_t oldParent = hcJson["Parent"].get<uint64_t>();
                if (oldParent != 0 && uuidMap.count(oldParent) > 0) {
                    hc.Parent = PixelEngine::UUID(uuidMap[oldParent]);
                } else {
                    hc.Parent = 0;
                }
                for (auto& childVal : hcJson["Children"]) {
                    uint64_t oldChild = childVal.get<uint64_t>();
                    if (uuidMap.count(oldChild) > 0) {
                        hc.Children.push_back(PixelEngine::UUID(uuidMap[oldChild]));
                    }
                }
            }

            if (entityJson.find("VelocityComponent") != entityJson.end()) {
                auto& vc = newEntity.AddComponent<PixelEngine::VelocityComponent>();
                auto& vcJson = entityJson["VelocityComponent"];
                vc.Linear = DeserializeVec3(vcJson["Linear"]);
                vc.Angular = DeserializeVec3(vcJson["Angular"]);
            }

            if (entityJson.find("SpriteAnimationComponent") != entityJson.end()) {
                auto& ac = newEntity.AddComponent<PixelEngine::SpriteAnimationComponent>();
                auto& acJson = entityJson["SpriteAnimationComponent"];
                for (auto& frameVal : acJson["Textures"]) {
                    ac.Textures.push_back(PixelEngine::UUID(frameVal.get<uint64_t>()));
                }
                ac.FrameTime = acJson.value("FrameTime", 0.1f);
                ac.Loop = acJson.value("Loop", true);
                ac.Playing = acJson.value("Playing", true);
            }

            if (entityJson.find("TilemapComponent") != entityJson.end()) {
                auto& tc = newEntity.AddComponent<PixelEngine::TilemapComponent>();
                auto& tmJson = entityJson["TilemapComponent"];
                tc.TilesetID = PixelEngine::UUID(tmJson.value("TilesetID", 0ull));
                tc.TileSize = tmJson.value("TileSize", 16u);
                tc.RenderLayer = tmJson.value("RenderLayer", 0);
                
                if (tmJson.contains("Chunks") && tmJson["Chunks"].is_array()) {
                    for (auto& chunkJson : tmJson["Chunks"]) {
                        int cx = chunkJson.value("x", 0);
                        int cy = chunkJson.value("y", 0);
                        PixelEngine::TilemapChunk chunk;
                        if (chunkJson.contains("Tiles") && chunkJson["Tiles"].is_array()) {
                            int idx = 0;
                            for (auto& tileVal : chunkJson["Tiles"]) {
                                if (idx < PixelEngine::TilemapChunk::ChunkSize * PixelEngine::TilemapChunk::ChunkSize) {
                                    chunk.Tiles[idx].TileIndex = tileVal.get<uint32_t>();
                                    idx++;
                                }
                            }
                        }
                        tc.Chunks[{cx, cy}] = chunk;
                    }
                }
            }

            if (entityJson.find("AnimatorComponent") != entityJson.end()) {
                auto& ac = newEntity.AddComponent<PixelEngine::AnimatorComponent>();
                auto& acJson = entityJson["AnimatorComponent"];
                ac.SpriteSheetID = PixelEngine::UUID(acJson.value("SpriteSheetID", 0ull));
                ac.CurrentClip = acJson.value("CurrentClip", "");
                ac.CurrentFrame = acJson.value("CurrentFrame", 0);
                ac.Playing = acJson.value("Playing", true);

                if (acJson.contains("Clips") && acJson["Clips"].is_array()) {
                    for (auto& clipJson : acJson["Clips"]) {
                        PixelEngine::AnimationClip clip;
                        clip.Name = clipJson.value("Name", "");
                        clip.FPS = clipJson.value("FPS", 10.0f);
                        clip.Loop = clipJson.value("Loop", true);
                        if (clipJson.contains("Frames") && clipJson["Frames"].is_array()) {
                            for (auto& frameJson : clipJson["Frames"]) {
                                PixelEngine::AnimationFrame frame;
                                frame.FrameName = frameJson.value("FrameName", "");
                                frame.EventName = frameJson.value("EventName", "");
                                clip.Frames.push_back(frame);
                            }
                        }
                        ac.Clips.push_back(clip);
                    }
                }
            }

            if (entityJson.find("AudioSourceComponent") != entityJson.end()) {
                auto& asc = newEntity.AddComponent<PixelEngine::AudioSourceComponent>();
                auto& audioJson = entityJson["AudioSourceComponent"];
                asc.ClipID = PixelEngine::UUID(audioJson.value("ClipID", 0ull));
                asc.Loop = audioJson.value("Loop", false);
                asc.PlayOnStart = audioJson.value("PlayOnStart", false);
                asc.Volume = audioJson.value("Volume", 1.0f);
                asc.IsMusic = audioJson.value("IsMusic", false);
                asc.Stream = nullptr;
                asc.IsPlaying = false;
            }

            auto& pc = newEntity.AddComponent<PixelEngine::PrefabComponent>();
            pc.PrefabID = prefabID;
            pc.OriginalUUID = PixelEngine::UUID(oldUUIDVal);
            if (entityJson.find("PrefabComponent") != entityJson.end()) {
                auto& pcJson = entityJson["PrefabComponent"];
                if (pcJson.contains("OverriddenFields") && pcJson["OverriddenFields"].is_array()) {
                    for (auto& fieldVal : pcJson["OverriddenFields"]) {
                        pc.OverriddenFields.push_back(fieldVal.get<std::string>());
                    }
                }
            }

            if (isFirst) {
                rootEntity = newEntity;
                isFirst = false;
            }
        }

        nlohmann::json afterState = serializer.SerializeToJson();
        PixelEngine::EditorHistory::PushCommand(
            std::make_unique<PixelEngine::SceneSnapshotCommand>(m_Context.ActiveScene, beforeState, afterState, "Instantiate Prefab")
        );

        m_Context.SelectedEntity = rootEntity;
    }

private:
    std::unique_ptr<PixelEngine::GraphicsPipeline> m_UpscalePipeline;
    std::unique_ptr<PixelEngine::Buffer> m_QuadVertexBuffer;
    std::unique_ptr<PixelEngine::Buffer> m_QuadIndexBuffer;
    
    std::unique_ptr<PixelEngine::RenderSystem> m_RenderSystem;

    PixelEngine::EditorContext m_Context;
    std::vector<std::unique_ptr<PixelEngine::EditorPanel>> m_Panels;
};

int main(int argc, char* argv[]) {
    try {
        EditorApp* app = new EditorApp();
        app->Run();
        delete app;
    } catch (const std::exception& e) {
        PX_CORE_CRITICAL("Unhandled Exception: {0}", e.what());
        return 1;
    } catch (...) {
        PX_CORE_CRITICAL("Unknown Exception occurred!");
        return 1;
    }
    return 0;
}
