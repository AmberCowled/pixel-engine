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

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#define ICON_FA_PLAY "\xef\x81\x8b"
#define ICON_FA_PAUSE "\xef\x81\x8c"
#define ICON_FA_STOP "\xef\x81\x8d"
#define ICON_FA_PLUS "\xef\x81\xa7"
#define ICON_FA_TRASH "\xef\x87\xb8"
#define ICON_FA_FOLDER "\xef\x81\xbb"
#define ICON_FA_FILE "\xef\x85\x9b"
#define ICON_FA_CAMERA "\xef\x80\xb0"
#define ICON_FA_VOLUME_HIGH "\xef\x80\xa8"
#define ICON_FA_GEAR "\xef\x80\x93"
#define ICON_FA_PAINT_BRUSH "\xef\x87\xbc"
#define ICON_FA_ERASER "\xef\x84\xad"
#define ICON_FA_CUBES "\xef\x86\xb3"
#define ICON_FA_CUBE "\xef\x86\xb2"
#define ICON_FA_IMAGE "\xef\x80\xbe"
#define ICON_FA_MAP "\xef\x89\xb9"
#define ICON_FA_FILM "\xef\x80\x88"
#define ICON_FA_SAVE "\xef\x83\x87"

#include <engine/core/EditorHistory.hpp>

class SceneSnapshotCommand : public PixelEngine::EditorCommand {
public:
    SceneSnapshotCommand(std::shared_ptr<PixelEngine::Scene>& scene, const nlohmann::json& oldState, const nlohmann::json& newState, const std::string& name)
        : m_Scene(scene), m_OldState(oldState), m_NewState(newState), m_Name(name) {}

    void Execute() override {
        PixelEngine::SceneSerializer serializer(*m_Scene);
        serializer.DeserializeFromJson(m_NewState);
    }

    void Undo() override {
        PixelEngine::SceneSerializer serializer(*m_Scene);
        serializer.DeserializeFromJson(m_OldState);
    }

    std::string GetName() const override { return m_Name; }

private:
    std::shared_ptr<PixelEngine::Scene>& m_Scene;
    nlohmann::json m_OldState;
    nlohmann::json m_NewState;
    std::string m_Name;
};

class SandboxApp : public PixelEngine::EngineApp {
public:
    SandboxApp() : PixelEngine::EngineApp({"Pixel Editor Workspace", 1280, 720}) {
        PX_INFO("Sandbox Editor Workspace Started.");
        
        auto& context = GetVulkanContext();

        // 1. Initialize global runtimes (Renderer2D, Audio)
        PixelEngine::Renderer2D::Init(context);
        PixelEngine::AudioManager::Init();

        // 2. Create Offscreen Target (default size 1280x720 scaled by DPI)
        float dpiScale = SDL_GetWindowDisplayScale(m_Window);
        PixelEngine::OffscreenTargetConfig offscreenConfig{};
        offscreenConfig.width = static_cast<uint32_t>(1280 * dpiScale);
        offscreenConfig.height = static_cast<uint32_t>(720 * dpiScale);
        offscreenConfig.colorFormat = VK_FORMAT_R8G8B8A8_SRGB;
        offscreenConfig.depthFormat = context.FindSupportedFormat(
            {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );

        m_OffscreenTarget = std::make_unique<PixelEngine::OffscreenTarget>(
            context, offscreenConfig, context.GetOffscreenRenderPass()
        );

        // 3. Link Offscreen Target to Upscale pipeline
        context.UpdateUpscaleDescriptorSets(m_OffscreenTarget->GetColorImageView());

        // 4. Create Geometry Buffers for Upscaling
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

        // 5. Create Upscale Pipeline
        PX_INFO("SandboxApp: Searching for upscale shaders...");
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
        PX_INFO("SandboxApp: Loading upscale shaders. Vert: {0}, Frag: {1}", upscaleVert, upscaleFrag);

        PixelEngine::PipelineConfigInfo upscaleConfig{};
        PixelEngine::GraphicsPipeline::DefaultPipelineConfigInfo(upscaleConfig);
        upscaleConfig.renderPass = context.GetRenderPass();
        upscaleConfig.pipelineLayout = context.GetUpscalePipelineLayout();
        upscaleConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
        upscaleConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        upscaleConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        PX_INFO("SandboxApp: Creating upscale pipeline...");
        m_UpscalePipeline = std::make_unique<PixelEngine::GraphicsPipeline>(
            context, upscaleVert, upscaleFrag, upscaleConfig
        );

        // 6. Initialize ECS Systems
        PX_INFO("SandboxApp: Creating RenderSystem...");
        m_RenderSystem = std::make_unique<PixelEngine::RenderSystem>(context);

        // 7. Load Recent Projects Configuration
        PX_INFO("SandboxApp: Loading recent projects list...");
        LoadRecentProjects();
        PX_INFO("SandboxApp: Constructor complete!");
    }

    ~SandboxApp() {
        auto& context = GetVulkanContext();
        vkDeviceWaitIdle(context.GetDevice());

        if (m_ViewportDescriptorSet != VK_NULL_HANDLE) {
            ImGui_ImplVulkan_RemoveTexture(m_ViewportDescriptorSet);
            m_ViewportDescriptorSet = VK_NULL_HANDLE;
        }

        PixelEngine::Renderer2D::Shutdown();
        PixelEngine::AssetManager::Shutdown();
        PixelEngine::AudioManager::Shutdown();
        PX_INFO("Sandbox App Shutdown.");
    }

    void OnUpdate(float deltaTime) override {
        // Draw warning modals for unsaved changes
        DrawExitPopup();
        DrawCloseProjectPopup();

        if (!m_ProjectLoaded) {
            DrawLauncher();
            return;
        }

        // Update window title dirty state indicator
        static bool lastDirty = false;
        bool currentDirty = PixelEngine::EditorHistory::IsDirty();
        if (currentDirty != lastDirty) {
            std::string title = "Pixel Editor Workspace - " + m_ProjectName;
            if (currentDirty) {
                title += " *";
            }
            SDL_SetWindowTitle(m_Window, title.c_str());
            lastDirty = currentDirty;
        }

        // Handle Undo / Redo keyboard shortcuts (Ctrl+Z / Ctrl+Y)
        ImGuiIO& io_shortcuts = ImGui::GetIO();
        if (!io_shortcuts.WantTextInput && m_SceneState == SceneState::Edit) {
            if (io_shortcuts.KeyCtrl) {
                if (ImGui::IsKeyPressed(ImGuiKey_Z)) {
                    PixelEngine::UUID selectedUUID = m_SelectedEntity ? m_SelectedEntity.GetComponent<PixelEngine::IDComponent>().ID : PixelEngine::UUID(0);
                    PixelEngine::EditorHistory::Undo();
                    m_SelectedEntity = (selectedUUID != PixelEngine::UUID(0)) ? m_ActiveScene->GetEntityByUUID(selectedUUID) : PixelEngine::Entity{};
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Y)) {
                    PixelEngine::UUID selectedUUID = m_SelectedEntity ? m_SelectedEntity.GetComponent<PixelEngine::IDComponent>().ID : PixelEngine::UUID(0);
                    PixelEngine::EditorHistory::Redo();
                    m_SelectedEntity = (selectedUUID != PixelEngine::UUID(0)) ? m_ActiveScene->GetEntityByUUID(selectedUUID) : PixelEngine::Entity{};
                }
            }
        }

        // Start ImGuizmo frame
        ImGuizmo::BeginFrame();

        // Run Asset Watcher (automatically handles shaders and textures)
        PixelEngine::AssetWatcher::Update();

        // Update ECS Systems (Velocity, Animations, etc.) if Playing
        if (m_SceneState == SceneState::Play) {
            m_ActiveScene->OnUpdate(deltaTime);
            PixelEngine::ScriptEngine::OnUpdate(deltaTime);
        } else if (m_SceneState == SceneState::Edit) {
            if (m_SelectedEntity && m_SelectedEntity.HasComponent<PixelEngine::AnimatorComponent>() && m_SelectedEntity.HasComponent<PixelEngine::SpriteRendererComponent>()) {
                auto& ac = m_SelectedEntity.GetComponent<PixelEngine::AnimatorComponent>();
                auto& sprite = m_SelectedEntity.GetComponent<PixelEngine::SpriteRendererComponent>();
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

        m_Rotation += deltaTime * m_RotationSpeed;

        auto& context = GetVulkanContext();

        // Keyboard shortcuts for Gizmos
        ImGuiIO& io = ImGui::GetIO();
        if (!io.WantTextInput && m_SceneState == SceneState::Edit) {
            if (ImGui::IsKeyPressed(ImGuiKey_Q)) m_GizmoType = -1;
            if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoType = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoType = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoType = ImGuizmo::SCALE;
        }

        // Dockspace Root
        static bool opt_fullscreen = true;
        static bool opt_padding = false;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->WorkPos);
            ImGui::SetNextWindowSize(viewport->WorkSize);
            ImGui::SetNextWindowViewport(viewport->ID);
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

            if (m_ResetLayout || !ImGui::DockBuilderGetNode(dockspace_id)) {
                m_ResetLayout = false;
                
                ImGui::DockBuilderRemoveNode(dockspace_id);
                ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
                
                const ImGuiViewport* imgui_viewport = ImGui::GetMainViewport();
                ImGui::DockBuilderSetNodeSize(dockspace_id, imgui_viewport->WorkSize);

                ImGuiID dock_main_id = dockspace_id;
                
                // 1. Split top for Toolbar (about 6% of height)
                ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.06f, nullptr, &dock_main_id);
                
                // 2. Split left for Hierarchy (about 20% of width)
                ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
                
                // 3. Split right for Inspector + Profiler (about 25% of width)
                ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
                
                // 4. Split bottom for Asset Browser + Console (about 30% of height)
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
                    std::filesystem::path projectPath(m_ProjectPath);
                    std::filesystem::path scenePath = projectPath / (m_ActiveScenePath.empty() ? "assets/scenes/startup.json" : m_ActiveScenePath);
                    std::filesystem::create_directories(scenePath.parent_path());
                    PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                    serializer.Serialize(scenePath.string());
                    PixelEngine::EditorHistory::SetDirty(false); // reset dirty flag
                    PX_INFO("Scene saved to: %s", scenePath.string().c_str());
                }
                if (ImGui::MenuItem("Close Project")) {
                    if (PixelEngine::EditorHistory::IsDirty()) {
                        m_ShowCloseProjectPopup = true;
                    } else {
                        m_ProjectLoaded = false;
                        m_ProjectPath = "";
                        m_ProjectName = "";
                        m_ActiveScenePath = "";
                        SDL_SetWindowTitle(m_Window, "Pixel Editor Workspace");
                    }
                }
                if (ImGui::MenuItem("Load Scene")) {
                    PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                    if (serializer.Deserialize("assets/scenes/sandbox_scene.json")) {
                        m_SelectedEntity = {};
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
                    m_ResetLayout = true;
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Game Object")) {
                if (ImGui::MenuItem("Create Empty Entity")) {
                    m_ActiveScene->CreateEntity("Empty Entity");
                }
                if (ImGui::MenuItem("Create Sprite Entity")) {
                    auto e = m_ActiveScene->CreateEntity("Sprite Entity");
                    e.AddComponent<PixelEngine::SpriteRendererComponent>();
                    e.GetComponent<PixelEngine::SpriteRendererComponent>().Mat.TextureID = testTexture;
                }
                if (ImGui::MenuItem("Create Cube Entity")) {
                    auto e = m_ActiveScene->CreateEntity("Cube Entity");
                    e.AddComponent<PixelEngine::MeshRendererComponent>();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Toolbar panel (Play / Pause / Stop)
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            float buttonSize = 40.0f;
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x * 0.5f - (buttonSize * 2.0f));
            
            // Play Button
            bool isPlay = (m_SceneState == SceneState::Play);
            if (ImGui::RadioButton(ICON_FA_PLAY " Play", isPlay)) {
                if (m_SceneState == SceneState::Edit) {
                    m_ActiveScene->StopAllAudio();
                    m_SceneState = SceneState::Play;
                    m_EditorScene = m_ActiveScene; // save original active scene
                    m_ActiveScene = PixelEngine::Scene::Clone(m_EditorScene);
                    m_SelectedEntity = {};

                    PixelEngine::ScriptEngine::SetActiveScene(m_ActiveScene);
                    auto view = m_ActiveScene->Reg().view<PixelEngine::IDComponent>();
                    for (auto entityID : view) {
                        PixelEngine::Entity entity = { entityID, m_ActiveScene.get() };
                        if (entity.HasComponent<PixelEngine::ScriptComponent>()) {
                            PixelEngine::ScriptEngine::OnCreateEntity(entity);
                        }
                    }
                } else if (m_SceneState == SceneState::Pause) {
                    m_SceneState = SceneState::Play;
                }
            }
            ImGui::SameLine();
            
            // Pause Button
            bool isPause = (m_SceneState == SceneState::Pause);
            if (ImGui::RadioButton(ICON_FA_PAUSE " Pause", isPause)) {
                if (m_SceneState == SceneState::Play) {
                    m_SceneState = SceneState::Pause;
                }
            }
            ImGui::SameLine();
            
            // Stop Button
            if (ImGui::Button(ICON_FA_STOP " Stop")) {
                if (m_SceneState == SceneState::Play || m_SceneState == SceneState::Pause) {
                    m_ActiveScene->StopAllAudio();
                    m_SceneState = SceneState::Edit;
                    m_ActiveScene = m_EditorScene; // restore edit scene
                    m_SelectedEntity = {};

                    PixelEngine::ScriptEngine::Reset();
                    PixelEngine::ScriptEngine::SetActiveScene(m_ActiveScene);
                }
            }
        }
        ImGui::End();

        // 1. Hierarchy Window
        ImGui::Begin("Hierarchy");
        {
            // Search input box
            ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", m_HierarchySearchBuffer, IM_ARRAYSIZE(m_HierarchySearchBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                m_HierarchySearchBuffer[0] = '\0';
            }

            // Filter chips
            auto filterChip = [&](const char* label, HierarchyFilter filterVal) {
                bool selected = (m_HierarchyFilter == filterVal);
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.40f, 0.94f, 1.0f)); // indigo highlight
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.25f, 0.36f, 1.0f)); // deep slate grey
                }
                if (ImGui::Button(label)) {
                    m_HierarchyFilter = filterVal;
                }
                ImGui::PopStyleColor();
            };

            filterChip("All", HierarchyFilter::All); ImGui::SameLine();
            filterChip("Script", HierarchyFilter::Script); ImGui::SameLine();
            filterChip("Sprite", HierarchyFilter::Sprite); ImGui::SameLine();
            filterChip("Audio", HierarchyFilter::Audio); ImGui::SameLine();
            filterChip("Tilemap", HierarchyFilter::Tilemap);
            ImGui::Separator();

            std::string searchQuery = m_HierarchySearchBuffer;
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
            bool isFiltering = !searchQuery.empty() || m_HierarchyFilter != HierarchyFilter::All;

            auto idView = m_ActiveScene->Reg().view<PixelEngine::IDComponent>();
            for (auto ent : idView) {
                PixelEngine::Entity entity = { ent, m_ActiveScene.get() };
                
                if (isFiltering) {
                    if (EntityMatchesFilters(entity, searchQuery)) {
                        // Draw flat list of nodes, forcing Leaf/NoTreePush flag
                        auto& tag = entity.GetComponent<PixelEngine::TagComponent>().Tag;
                        auto myUUID = entity.GetComponent<PixelEngine::IDComponent>().ID;
                        ImGuiTreeNodeFlags flags = ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                        
                        std::string icon = "";
                        if (entity.HasComponent<PixelEngine::AudioSourceComponent>()) icon = ICON_FA_VOLUME_HIGH " ";
                        else if (entity.HasComponent<PixelEngine::TilemapComponent>()) icon = ICON_FA_MAP " ";
                        else if (entity.HasComponent<PixelEngine::AnimatorComponent>()) icon = ICON_FA_FILM " ";
                        else if (entity.HasComponent<PixelEngine::SpriteRendererComponent>()) icon = ICON_FA_IMAGE " ";
                        else if (entity.HasComponent<PixelEngine::MeshRendererComponent>()) icon = ICON_FA_CUBES " ";
                        else icon = ICON_FA_GEAR " ";

                        std::string label = icon + tag;
                        PixelEngine::UUID prefabID = 0;
                        bool isPrefab = IsPartOfPrefab(entity, prefabID);
                        if (isPrefab) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.376f, 0.647f, 0.980f, 1.0f));

                        ImGui::TreeNodeEx((void*)(uint64_t)myUUID, flags, "%s", label.c_str());
                        if (isPrefab) ImGui::PopStyleColor();

                        if (ImGui::IsItemClicked()) {
                            m_SelectedEntity = entity;
                        }
                    }
                } else {
                    bool isRoot = true;
                    if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
                        isRoot = (entity.GetComponent<PixelEngine::HierarchyComponent>().Parent == 0);
                    }
                    if (isRoot) {
                        DrawEntityNode(entity);
                    }
                }
            }

            // Drop target on the panel background to clear parent
            ImGui::Dummy(ImGui::GetContentRegionAvail());
            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID")) {
                    PixelEngine::UUID draggedUUID = *(const PixelEngine::UUID*)payload->Data;
                    auto draggedEntity = m_ActiveScene->GetEntityByUUID(draggedUUID);
                    if (draggedEntity && draggedEntity.HasComponent<PixelEngine::HierarchyComponent>()) {
                        auto& hc = draggedEntity.GetComponent<PixelEngine::HierarchyComponent>();
                        if (hc.Parent != 0) {
                            auto oldParent = m_ActiveScene->GetEntityByUUID(hc.Parent);
                            if (oldParent && oldParent.HasComponent<PixelEngine::HierarchyComponent>()) {
                                auto& oldParentHc = oldParent.GetComponent<PixelEngine::HierarchyComponent>();
                                oldParentHc.Children.erase(std::remove(oldParentHc.Children.begin(), oldParentHc.Children.end(), draggedUUID), oldParentHc.Children.end());
                            }
                            hc.Parent = 0;
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImGui::Separator();
            if (ImGui::Button("Add Entity")) {
                PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                nlohmann::json beforeState = serializer.SerializeToJson();
                
                auto ent = m_ActiveScene->CreateEntity("New Entity");
                m_SelectedEntity = ent;
                
                nlohmann::json afterState = serializer.SerializeToJson();
                PixelEngine::EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_ActiveScene, beforeState, afterState, "Add Entity")
                );
            }
        }
        ImGui::End();

        // 2. Inspector Window
        ImGui::Begin("Inspector");
        if (m_SelectedEntity) {
            PixelEngine::UUID prefabID = 0;
            if (IsPartOfPrefab(m_SelectedEntity, prefabID)) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.376f, 0.647f, 0.980f, 1.0f));
                ImGui::Text(ICON_FA_CUBE " Prefab Instance");
                ImGui::PopStyleColor();
                ImGui::SameLine();
                if (ImGui::Button("Apply Overrides")) {
                    ApplyPrefabOverrides(m_SelectedEntity, prefabID);
                }
                ImGui::SameLine();
                if (ImGui::Button("Revert Overrides")) {
                    RevertPrefabOverrides(m_SelectedEntity, prefabID);
                }
                ImGui::Separator();
            }

            auto& tag = m_SelectedEntity.GetComponent<PixelEngine::TagComponent>();
            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strncpy_s(buffer, tag.Tag.c_str(), sizeof(buffer));
            if (ImGui::InputText("Tag", buffer, sizeof(buffer))) {
                tag.Tag = std::string(buffer);
                TrackOverride(m_SelectedEntity, "TagComponent.Tag");
            }

            ImGui::Separator();

            if (m_SelectedEntity.HasComponent<PixelEngine::TransformComponent>()) {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& tc = m_SelectedEntity.GetComponent<PixelEngine::TransformComponent>();
                    
                    static nlohmann::json beforeEditState;
                    static bool isEditing = false;
                    
                    bool changed = false;
                    if (ImGui::DragFloat3("Position", &tc.Translation.x, 0.1f)) {
                        changed = true;
                        TrackOverride(m_SelectedEntity, "TransformComponent.Translation");
                    }
                    if (ImGui::DragFloat3("Rotation", &tc.Rotation.x, 0.1f)) {
                        changed = true;
                        TrackOverride(m_SelectedEntity, "TransformComponent.Rotation");
                    }
                    if (ImGui::DragFloat3("Scale", &tc.Scale.x, 0.1f)) {
                        changed = true;
                        TrackOverride(m_SelectedEntity, "TransformComponent.Scale");
                    }

                    if (changed && !isEditing) {
                        PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                        beforeEditState = serializer.SerializeToJson();
                        isEditing = true;
                    }
                    
                    if (isEditing && (ImGui::IsItemDeactivatedAfterEdit() || (!ImGui::IsItemActive() && !ImGui::IsAnyItemActive()))) {
                        PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                        nlohmann::json afterEditState = serializer.SerializeToJson();
                        PixelEngine::EditorHistory::PushCommand(
                            std::make_unique<SceneSnapshotCommand>(m_ActiveScene, beforeEditState, afterEditState, "Modify Transform")
                        );
                        isEditing = false;
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::MeshRendererComponent>()) {
                if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& mc = m_SelectedEntity.GetComponent<PixelEngine::MeshRendererComponent>();
                    if (ImGui::ColorEdit4("Mesh Color", &mc.Color.x)) {
                        TrackOverride(m_SelectedEntity, "MeshRendererComponent.Color");
                    }
                    
                    ImGui::Text("Texture UUID: %llu", (uint64_t)mc.TextureID);
                    if (mc.TextureID != 0) {
                        ImGui::Text("Path: %s", PixelEngine::AssetManager::GetAssetPath(mc.TextureID).c_str());
                    } else {
                        ImGui::Text("Path: None (Default white)");
                    }
                    
                    ImGui::Button("Drag texture here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            PixelEngine::UUID uuid = PixelEngine::AssetManager::LoadTexture(assetPath);
                            if (uuid != 0) {
                                mc.TextureID = uuid;
                                TrackOverride(m_SelectedEntity, "MeshRendererComponent.TextureID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::SpriteRendererComponent>()) {
                if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& sc = m_SelectedEntity.GetComponent<PixelEngine::SpriteRendererComponent>();
                    if (ImGui::ColorEdit4("Sprite Color", &sc.Mat.Color.x)) {
                        TrackOverride(m_SelectedEntity, "SpriteRendererComponent.Color");
                    }

                    const char* blendModes[] = { "Opaque", "AlphaBlend", "Additive" };
                    int currentBlend = static_cast<int>(sc.Mat.Blend);
                    if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, 3)) {
                        sc.Mat.Blend = static_cast<PixelEngine::BlendMode>(currentBlend);
                        TrackOverride(m_SelectedEntity, "SpriteRendererComponent.Blend");
                    }
                    
                    ImGui::Text("Texture UUID: %llu", (uint64_t)sc.Mat.TextureID);
                    if (sc.Mat.TextureID != 0) {
                        ImGui::Text("Path: %s", PixelEngine::AssetManager::GetAssetPath(sc.Mat.TextureID).c_str());
                    } else {
                        ImGui::Text("Path: None (Default white)");
                    }
                    
                    ImGui::Button("Drag texture here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            PixelEngine::UUID uuid = PixelEngine::AssetManager::LoadTexture(assetPath);
                            if (uuid != 0) {
                                sc.Mat.TextureID = uuid;
                                TrackOverride(m_SelectedEntity, "SpriteRendererComponent.TextureID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::VelocityComponent>()) {
                if (ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& vc = m_SelectedEntity.GetComponent<PixelEngine::VelocityComponent>();
                    if (ImGui::DragFloat3("Linear", &vc.Linear.x, 0.05f)) {
                        TrackOverride(m_SelectedEntity, "VelocityComponent.Linear");
                    }
                    if (ImGui::DragFloat3("Angular", &vc.Angular.x, 0.05f)) {
                        TrackOverride(m_SelectedEntity, "VelocityComponent.Angular");
                    }
                    if (ImGui::Button("Remove Velocity")) {
                        m_SelectedEntity.RemoveComponent<PixelEngine::VelocityComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::HierarchyComponent>()) {
                if (ImGui::CollapsingHeader("Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& hc = m_SelectedEntity.GetComponent<PixelEngine::HierarchyComponent>();
                    if (hc.Parent != 0) {
                        auto parentEntity = m_ActiveScene->GetEntityByUUID(hc.Parent);
                        if (parentEntity) {
                            auto& parentTag = parentEntity.GetComponent<PixelEngine::TagComponent>();
                            ImGui::Text("Parent: %s", parentTag.Tag.c_str());
                        } else {
                            ImGui::Text("Parent UUID: %llu (not found)", (uint64_t)hc.Parent);
                        }
                        if (ImGui::Button("Clear Parent")) {
                            auto parentEnt = m_ActiveScene->GetEntityByUUID(hc.Parent);
                            if (parentEnt && parentEnt.HasComponent<PixelEngine::HierarchyComponent>()) {
                                auto& parentHc = parentEnt.GetComponent<PixelEngine::HierarchyComponent>();
                                auto myUUID = m_SelectedEntity.GetComponent<PixelEngine::IDComponent>().ID;
                                parentHc.Children.erase(std::remove(parentHc.Children.begin(), parentHc.Children.end(), myUUID), parentHc.Children.end());
                            }
                            hc.Parent = 0;
                        }
                    } else {
                        ImGui::Text("No Parent");
                    }
                    
                    if (ImGui::Button("Remove Hierarchy Component")) {
                        m_SelectedEntity.RemoveComponent<PixelEngine::HierarchyComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::SpriteAnimationComponent>()) {
                if (ImGui::CollapsingHeader("Sprite Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& ac = m_SelectedEntity.GetComponent<PixelEngine::SpriteAnimationComponent>();
                    if (ImGui::Checkbox("Playing", &ac.Playing)) {
                        TrackOverride(m_SelectedEntity, "SpriteAnimationComponent.Playing");
                    }
                    if (ImGui::Checkbox("Loop", &ac.Loop)) {
                        TrackOverride(m_SelectedEntity, "SpriteAnimationComponent.Loop");
                    }
                    if (ImGui::SliderFloat("Frame Duration", &ac.FrameTime, 0.05f, 2.0f)) {
                        TrackOverride(m_SelectedEntity, "SpriteAnimationComponent.FrameTime");
                    }
                    ImGui::Text("Frame Count: %d", (int)ac.Textures.size());
                    
                    if (ImGui::Button("Setup Test Anim (Blink)")) {
                        ac.Textures.clear();
                        ac.Textures.push_back(testTexture);
                        ac.Textures.push_back(0); // none texture ID
                        ac.CurrentFrame = 0;
                        ac.Timer = 0.0f;
                        ac.Playing = true;
                        TrackOverride(m_SelectedEntity, "SpriteAnimationComponent.Textures");
                        TrackOverride(m_SelectedEntity, "SpriteAnimationComponent.Playing");
                    }
                    if (ImGui::Button("Remove Animation")) {
                        m_SelectedEntity.RemoveComponent<PixelEngine::SpriteAnimationComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::TilemapComponent>()) {
                if (ImGui::CollapsingHeader("Tilemap Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& tc = m_SelectedEntity.GetComponent<PixelEngine::TilemapComponent>();
                    ImGui::Text("Tileset UUID: %llu", (uint64_t)tc.TilesetID);
                    if (tc.TilesetID != 0) {
                        ImGui::Text("Path: %s", PixelEngine::AssetManager::GetAssetPath(tc.TilesetID).c_str());
                    } else {
                        ImGui::Text("Path: None");
                    }
                    
                    ImGui::Button("Drag tileset here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            PixelEngine::UUID uuid = PixelEngine::AssetManager::LoadTileset(assetPath);
                            if (uuid != 0) {
                                tc.TilesetID = uuid;
                                TrackOverride(m_SelectedEntity, "TilemapComponent.TilesetID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    int renderLayer = tc.RenderLayer;
                    if (ImGui::DragInt("Render Layer", &renderLayer, 1.0f)) {
                        tc.RenderLayer = renderLayer;
                        TrackOverride(m_SelectedEntity, "TilemapComponent.RenderLayer");
                    }

                    int tileSize = (int)tc.TileSize;
                    if (ImGui::DragInt("Tile Size", &tileSize, 1.0f, 1, 128)) {
                        tc.TileSize = static_cast<uint32_t>(tileSize);
                        TrackOverride(m_SelectedEntity, "TilemapComponent.TileSize");
                    }

                    if (ImGui::Button("Remove Tilemap")) {
                        m_SelectedEntity.RemoveComponent<PixelEngine::TilemapComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::AnimatorComponent>()) {
                if (ImGui::CollapsingHeader("Animator Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& ac = m_SelectedEntity.GetComponent<PixelEngine::AnimatorComponent>();
                    ImGui::Text("Spritesheet UUID: %llu", (uint64_t)ac.SpriteSheetID);
                    if (ac.SpriteSheetID != 0) {
                        ImGui::Text("Path: %s", PixelEngine::AssetManager::GetAssetPath(ac.SpriteSheetID).c_str());
                    } else {
                        ImGui::Text("Path: None");
                    }
                    
                    ImGui::Button("Drag spritesheet here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            PixelEngine::UUID uuid = PixelEngine::AssetManager::LoadSpriteSheet(assetPath);
                            if (uuid != 0) {
                                ac.SpriteSheetID = uuid;
                                TrackOverride(m_SelectedEntity, "AnimatorComponent.SpriteSheetID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (ImGui::Checkbox("Playing", &ac.Playing)) {
                        TrackOverride(m_SelectedEntity, "AnimatorComponent.Playing");
                    }
                    ImGui::Text("Current Clip: %s", ac.CurrentClip.c_str());
                    ImGui::Text("Current Frame: %d", ac.CurrentFrame);

                    if (ImGui::Button("Remove Animator")) {
                        m_SelectedEntity.RemoveComponent<PixelEngine::AnimatorComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::AudioSourceComponent>()) {
                if (ImGui::CollapsingHeader("Audio Source Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& asc = m_SelectedEntity.GetComponent<PixelEngine::AudioSourceComponent>();
                    ImGui::Text("Audio Clip UUID: %llu", (uint64_t)asc.ClipID);
                    if (asc.ClipID != 0) {
                        ImGui::Text("Path: %s", PixelEngine::AssetManager::GetAssetPath(asc.ClipID).c_str());
                    } else {
                        ImGui::Text("Path: None");
                    }
                    
                    ImGui::Button("Drag .wav here to assign", ImVec2(-1, 30));
                    if (ImGui::BeginDragDropTarget()) {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                            const char* assetPath = (const char*)payload->Data;
                            PixelEngine::UUID uuid = PixelEngine::AssetManager::LoadAudioClip(assetPath);
                            if (uuid != 0) {
                                asc.ClipID = uuid;
                                TrackOverride(m_SelectedEntity, "AudioSourceComponent.ClipID");
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (ImGui::Checkbox("Looping", &asc.Loop)) {
                        TrackOverride(m_SelectedEntity, "AudioSourceComponent.Loop");
                    }
                    if (ImGui::Checkbox("Play On Start", &asc.PlayOnStart)) {
                        TrackOverride(m_SelectedEntity, "AudioSourceComponent.PlayOnStart");
                    }
                    if (ImGui::Checkbox("Is Music Bus", &asc.IsMusic)) {
                        TrackOverride(m_SelectedEntity, "AudioSourceComponent.IsMusic");
                    }

                    float volume = asc.Volume;
                    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
                        asc.Volume = volume;
                        TrackOverride(m_SelectedEntity, "AudioSourceComponent.Volume");
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
                        m_SelectedEntity.RemoveComponent<PixelEngine::AudioSourceComponent>();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::ScriptComponent>()) {
                if (ImGui::CollapsingHeader("Script Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& sc = m_SelectedEntity.GetComponent<PixelEngine::ScriptComponent>();
                    char scriptBuffer[256];
                    memset(scriptBuffer, 0, sizeof(scriptBuffer));
                    strncpy_s(scriptBuffer, sc.ClassName.c_str(), sizeof(scriptBuffer));
                    if (ImGui::InputText("Class Name", scriptBuffer, sizeof(scriptBuffer))) {
                        sc.ClassName = std::string(scriptBuffer);
                        TrackOverride(m_SelectedEntity, "ScriptComponent.ClassName");
                    }
                    if (ImGui::Button("Remove Script Component")) {
                        m_SelectedEntity.RemoveComponent<PixelEngine::ScriptComponent>();
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
                ImGui::OpenPopup("AddComponentPopup");
            }
            
            if (ImGui::BeginPopup("AddComponentPopup")) {
                auto addComponentHelper = [&](auto componentType, const std::string& name) {
                    PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                    nlohmann::json beforeState = serializer.SerializeToJson();
                    
                    m_SelectedEntity.AddComponent<decltype(componentType)>();
                    
                    nlohmann::json afterState = serializer.SerializeToJson();
                    PixelEngine::EditorHistory::PushCommand(
                        std::make_unique<SceneSnapshotCommand>(m_ActiveScene, beforeState, afterState, "Add Component: " + name)
                    );
                    ImGui::CloseCurrentPopup();
                };

                if (!m_SelectedEntity.HasComponent<PixelEngine::TransformComponent>() && ImGui::MenuItem("Transform")) {
                    addComponentHelper(PixelEngine::TransformComponent{}, "Transform");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::SpriteRendererComponent>() && ImGui::MenuItem("Sprite Renderer")) {
                    addComponentHelper(PixelEngine::SpriteRendererComponent{}, "Sprite Renderer");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::MeshRendererComponent>() && ImGui::MenuItem("Mesh Renderer")) {
                    addComponentHelper(PixelEngine::MeshRendererComponent{}, "Mesh Renderer");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::VelocityComponent>() && ImGui::MenuItem("Velocity")) {
                    addComponentHelper(PixelEngine::VelocityComponent{}, "Velocity");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::HierarchyComponent>() && ImGui::MenuItem("Hierarchy")) {
                    addComponentHelper(PixelEngine::HierarchyComponent{}, "Hierarchy");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::SpriteAnimationComponent>() && ImGui::MenuItem("Sprite Animation")) {
                    addComponentHelper(PixelEngine::SpriteAnimationComponent{}, "Sprite Animation");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::TilemapComponent>() && ImGui::MenuItem("Tilemap Component")) {
                    addComponentHelper(PixelEngine::TilemapComponent{}, "Tilemap");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::AnimatorComponent>() && ImGui::MenuItem("Animator Component")) {
                    addComponentHelper(PixelEngine::AnimatorComponent{}, "Animator");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::AudioSourceComponent>() && ImGui::MenuItem("Audio Source Component")) {
                    addComponentHelper(PixelEngine::AudioSourceComponent{}, "Audio Source");
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::ScriptComponent>() && ImGui::MenuItem("Script Component")) {
                    addComponentHelper(PixelEngine::ScriptComponent{}, "Script");
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();
            if (ImGui::Button("Delete Entity", ImVec2(-1, 30))) {
                PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                nlohmann::json beforeState = serializer.SerializeToJson();
                
                m_ActiveScene->DestroyEntity(m_SelectedEntity);
                m_SelectedEntity = {};
                
                nlohmann::json afterState = serializer.SerializeToJson();
                PixelEngine::EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_ActiveScene, beforeState, afterState, "Delete Entity")
                );
            }
        }
        ImGui::End();

        // 3. Profiler / Stats Window
        ImGui::Begin("Profiler");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("CPU Frame Time: %.3f ms", deltaTime * 1000.0f);
        ImGui::Text("GPU Frame Time: %.3f ms", context.GetGPUTime());
        
        auto stats = PixelEngine::Renderer2D::GetStats();
        ImGui::Text("Batch Draw Calls: %u", stats.DrawCalls);
        ImGui::Text("Batch Quad Count: %u", stats.QuadCount);

        ImGui::Separator();
        ImGui::Text("ECS Runtime:");
        ImGui::Text("Active Entities: %zu", m_ActiveScene->Reg().storage<entt::entity>().size());

        ImGui::Separator();
        ImGui::Text("Resource Lifetime Stats:");
        ImGui::Text("Loaded Textures: %zu", PixelEngine::AssetManager::GetLoadedTexturesCount());
        ImGui::Text("Loaded Tilesets: %zu", PixelEngine::AssetManager::GetLoadedTilesetsCount());
        ImGui::Text("Loaded SpriteSheets: %zu", PixelEngine::AssetManager::GetLoadedSpriteSheetsCount());
        ImGui::Text("Loaded AudioClips: %zu", PixelEngine::AssetManager::GetLoadedAudioClipsCount());
        
        ImGui::Separator();
        
        ImGui::Checkbox("Pixel Snapping", &m_PixelSnapping);
        ImGui::SliderFloat("Rotation Speed", &m_RotationSpeed, 0.0f, 200.0f);
        ImGui::End();

        // 4. Asset Browser Window
        ImGui::Begin("Asset Browser");
        {
            std::filesystem::path assetRoot = m_ProjectLoaded ? (std::filesystem::path(m_ProjectPath) / "assets") : std::filesystem::path("assets");
            if (m_CurrentDirectory.empty() || !std::filesystem::exists(m_CurrentDirectory)) {
                m_CurrentDirectory = assetRoot;
            }

            // Search input box
            ImGui::InputTextWithHint("##AssetSearch", "Search assets...", m_AssetSearchBuffer, IM_ARRAYSIZE(m_AssetSearchBuffer));
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                m_AssetSearchBuffer[0] = '\0';
            }

            // Filter chips
            auto assetFilterChip = [&](const char* label, AssetFilter filterVal) {
                bool selected = (m_AssetFilter == filterVal);
                if (selected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.39f, 0.40f, 0.94f, 1.0f)); // indigo highlight
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.25f, 0.36f, 1.0f)); // deep slate grey
                }
                if (ImGui::Button(label)) {
                    m_AssetFilter = filterVal;
                }
                ImGui::PopStyleColor();
            };

            assetFilterChip("All", AssetFilter::All); ImGui::SameLine();
            assetFilterChip("Textures", AssetFilter::Textures); ImGui::SameLine();
            assetFilterChip("Audio", AssetFilter::Audio); ImGui::SameLine();
            assetFilterChip("Prefabs", AssetFilter::Prefabs); ImGui::SameLine();
            assetFilterChip("Scenes", AssetFilter::Scenes);
            ImGui::Separator();

            std::string searchQuery = m_AssetSearchBuffer;
            std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
            bool isSearching = !searchQuery.empty() || m_AssetFilter != AssetFilter::All;

            if (!isSearching) {
                // Draw navigation breadcrumbs
                if (m_CurrentDirectory != assetRoot) {
                    if (ImGui::Button("<- Back")) {
                        m_CurrentDirectory = m_CurrentDirectory.parent_path();
                    }
                    ImGui::SameLine();
                    ImGui::Text("Current Path: %s", std::filesystem::relative(m_CurrentDirectory, assetRoot).string().c_str());
                } else {
                    ImGui::Text("Assets Root");
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
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(assetRoot)) {
                        if (entry.is_regular_file()) {
                            auto path = entry.path();
                            if (MatchAssetFilter(path) && MatchAssetSearch(path, searchQuery)) {
                                ImGui::TableNextColumn();
                                DrawAssetGridItem(path, assetRoot);
                            }
                        }
                    }
                } else {
                    // 1. Draw directories
                    for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
                        if (entry.is_directory()) {
                            ImGui::TableNextColumn();
                            DrawDirectoryGridItem(entry.path(), assetRoot);
                        }
                    }
                    // 2. Draw files
                    for (const auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
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
        }
        ImGui::End();

        // 5. Console Window
        ImGui::Begin("Console");
        if (ImGui::Button("Clear")) {
            PixelEngine::EditorConsoleSink::Clear();
        }
        ImGui::Separator();
        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        {
            const auto& logs = PixelEngine::EditorConsoleSink::GetMessages();
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

        // 6. Viewport Window
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
        ImGui::Begin("Viewport");
        {
            m_ViewportFocused = ImGui::IsWindowFocused();
            m_ViewportHovered = ImGui::IsWindowHovered();

            ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
            if (viewportPanelSize.x != m_ViewportSize.x || viewportPanelSize.y != m_ViewportSize.y) {
                m_ViewportSize = viewportPanelSize;
                float dpiScale = SDL_GetWindowDisplayScale(m_Window);
                uint32_t width = std::max((uint32_t)(m_ViewportSize.x * dpiScale), 64u);
                uint32_t height = std::max((uint32_t)(m_ViewportSize.y * dpiScale), 64u);

                vkDeviceWaitIdle(context.GetDevice());
                
                PixelEngine::OffscreenTargetConfig offscreenConfig{};
                offscreenConfig.width = width;
                offscreenConfig.height = height;
                offscreenConfig.colorFormat = VK_FORMAT_R8G8B8A8_SRGB;
                offscreenConfig.depthFormat = context.FindSupportedFormat(
                    {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                );

                m_OffscreenTarget = std::make_unique<PixelEngine::OffscreenTarget>(
                    context, offscreenConfig, context.GetOffscreenRenderPass()
                );

                context.UpdateUpscaleDescriptorSets(m_OffscreenTarget->GetColorImageView());

                // Recreate ImGui viewport descriptor set
                if (m_ViewportDescriptorSet != VK_NULL_HANDLE) {
                    ImGui_ImplVulkan_RemoveTexture(m_ViewportDescriptorSet);
                }
                m_ViewportDescriptorSet = ImGui_ImplVulkan_AddTexture(
                    context.GetTextureSampler(),
                    m_OffscreenTarget->GetColorImageView(),
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                );
            }

            if (m_ViewportDescriptorSet != VK_NULL_HANDLE) {
                ImGui::Image((ImTextureID)m_ViewportDescriptorSet, ImVec2{ m_ViewportSize.x, m_ViewportSize.y });
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
                    const char* assetPath = (const char*)payload->Data;
                    std::string pathStr(assetPath);
                    if (pathStr.ends_with(".prefab.json") || pathStr.ends_with(".prefab")) {
                        InstantiatePrefab(pathStr);
                    }
                }
                ImGui::EndDragDropTarget();
            }

            ImVec2 imageMin = ImGui::GetItemRectMin();
            ImVec2 imageSize = ImGui::GetItemRectSize();

            if (m_SelectedEntity && m_SelectedEntity.HasComponent<PixelEngine::TilemapComponent>() && m_SceneState == SceneState::Edit) {
                if (m_ViewportHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
                    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                        PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                        m_BeforePaintState = serializer.SerializeToJson();
                        m_IsPainting = true;
                    }

                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && m_IsPainting) {
                        ImVec2 mousePos = ImGui::GetMousePos();
                        float mx = mousePos.x - imageMin.x;
                        float my = mousePos.y - imageMin.y;
                        
                        if (mx >= 0.0f && mx < imageSize.x && my >= 0.0f && my < imageSize.y) {
                            float ndcX = (mx / imageSize.x) * 2.0f - 1.0f;
                            float ndcY = 1.0f - (my / imageSize.y) * 2.0f;
                            
                            glm::mat4 projection = m_Camera.GetProjection();
                            glm::mat4 view = m_Camera.GetView();
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
                                    
                                    glm::mat4 invWorldTransform = glm::inverse(m_ActiveScene->GetWorldTransform(m_SelectedEntity));
                                    glm::vec4 localIntersection = invWorldTransform * glm::vec4(intersection, 1.0f);
                                    
                                    int tileX = static_cast<int>(std::floor(localIntersection.x));
                                    int tileY = static_cast<int>(std::floor(localIntersection.y));
                                    
                                    int chunkX = tileX >= 0 ? tileX / 16 : (tileX - 15) / 16;
                                    int chunkY = tileY >= 0 ? tileY / 16 : (tileY - 15) / 16;
                                    int localX = tileX - chunkX * 16;
                                    int localY = tileY - chunkY * 16;
                                    
                                    auto& tc = m_SelectedEntity.GetComponent<PixelEngine::TilemapComponent>();
                                    auto chunkCoords = std::make_pair(chunkX, chunkY);
                                    
                                    if (m_BrushType == BrushType::Paint) {
                                        tc.Chunks[chunkCoords].Tiles[localY * 16 + localX].TileIndex = m_SelectedTileIndex;
                                        TrackOverride(m_SelectedEntity, "TilemapComponent.Chunks");
                                    } else if (m_BrushType == BrushType::Erase) {
                                        tc.Chunks[chunkCoords].Tiles[localY * 16 + localX].TileIndex = 0;
                                        TrackOverride(m_SelectedEntity, "TilemapComponent.Chunks");
                                    }
                                }
                            }
                        }
                    }
                }
            }

            if (m_IsPainting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
                PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                nlohmann::json afterPaintState = serializer.SerializeToJson();
                PixelEngine::EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_ActiveScene, m_BeforePaintState, afterPaintState, "Paint Tilemap")
                );
                m_IsPainting = false;
            }

            // Editor 2D Viewport Camera Controls (Pan & Zoom centered on cursor)
            if (m_ViewportHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
                // Determine mouse pos under cursor for zoom centering
                ImVec2 mousePos = ImGui::GetMousePos();
                float mx = mousePos.x - imageMin.x;
                float my = mousePos.y - imageMin.y;

                glm::vec3 worldMouseBefore(0.0f);
                bool hasMousePos = false;

                if (mx >= 0.0f && mx < imageSize.x && my >= 0.0f && my < imageSize.y) {
                    float ndcX = (mx / imageSize.x) * 2.0f - 1.0f;
                    float ndcY = 1.0f - (my / imageSize.y) * 2.0f;

                    glm::mat4 projection = m_Camera.GetProjection();
                    glm::mat4 view = m_Camera.GetView();
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
                    float oldDistance = m_CameraDistance;
                    m_CameraDistance -= io.MouseWheel * 0.25f * (m_CameraDistance * 0.1f);
                    m_CameraDistance = glm::clamp(m_CameraDistance, 0.5f, 100.0f);

                    // Adjust camera target to keep mouse cursor over the same world position
                    if (hasMousePos) {
                        m_CameraTarget.x = worldMouseBefore.x - (m_CameraDistance / oldDistance) * (worldMouseBefore.x - m_CameraTarget.x);
                        m_CameraTarget.y = worldMouseBefore.y - (m_CameraDistance / oldDistance) * (worldMouseBefore.y - m_CameraTarget.y);
                    }
                }

                // Handle Pan
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) || ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                    ImVec2 mouseDelta = io.MouseDelta;
                    float aspect = imageSize.x / imageSize.y;
                    float tanHalfFovy = tan(glm::radians(45.0f) / 2.0f);
                    float worldUnitsPerPixelY = 2.0f * m_CameraDistance * tanHalfFovy / imageSize.y;
                    float worldUnitsPerPixelX = worldUnitsPerPixelY * aspect;

                    m_CameraTarget.x += mouseDelta.x * worldUnitsPerPixelX;
                    m_CameraTarget.y += mouseDelta.y * worldUnitsPerPixelY;
                }
            }

            // Keyboard shortcut for Focus selection (F key)
            if (m_SelectedEntity && m_SelectedEntity.HasComponent<PixelEngine::TransformComponent>() && ImGui::IsKeyPressed(ImGuiKey_F) && !io.WantTextInput) {
                auto& tc = m_SelectedEntity.GetComponent<PixelEngine::TransformComponent>();
                m_CameraTarget.x = tc.Translation.x;
                m_CameraTarget.y = tc.Translation.y;
            }

            // Lock pitch and yaw to view straight down Z axis
            glm::vec3 cameraPos = glm::vec3(m_CameraTarget.x, m_CameraTarget.y, m_CameraDistance);
            m_Camera.SetViewTarget(cameraPos, glm::vec3(m_CameraTarget.x, m_CameraTarget.y, 0.0f));

            // ImGuizmo Manipulator
            if (m_SelectedEntity && m_GizmoType >= 0 && m_SceneState == SceneState::Edit) {
                ImGuizmo::SetOrthographic(false);
                ImGuizmo::SetDrawlist();
                
                ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, ImGui::GetWindowWidth(), ImGui::GetWindowHeight());
                
                glm::mat4 cameraProjection = m_Camera.GetProjection();
                glm::mat4 cameraView = m_Camera.GetView();
                
                auto& tc = m_SelectedEntity.GetComponent<PixelEngine::TransformComponent>();
                glm::mat4 transform = tc.GetTransform();
                
                bool snap = io.KeyCtrl;
                float snapValue = 0.5f;
                if (m_GizmoType == ImGuizmo::TRANSLATE) {
                    snapValue = 0.5f;
                } else if (m_GizmoType == ImGuizmo::ROTATE) {
                    snapValue = 45.0f;
                }
                float snapValues[3] = { snapValue, snapValue, snapValue };
                
                ImGuizmo::Manipulate(
                    glm::value_ptr(cameraView), 
                    glm::value_ptr(cameraProjection), 
                    (ImGuizmo::OPERATION)m_GizmoType, 
                    ImGuizmo::LOCAL, 
                    glm::value_ptr(transform), 
                    nullptr, 
                    snap ? snapValues : nullptr
                );
                
                static nlohmann::json beforeGizmoState;
                static bool wasGizmoUsing = false;

                if (ImGuizmo::IsUsing()) {
                    if (!wasGizmoUsing) {
                        PixelEngine::SceneSerializer serializer(*m_ActiveScene);
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
                        PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                        nlohmann::json afterGizmoState = serializer.SerializeToJson();
                        PixelEngine::EditorHistory::PushCommand(
                            std::make_unique<SceneSnapshotCommand>(m_ActiveScene, beforeGizmoState, afterGizmoState, "Gizmo Transform")
                        );
                        wasGizmoUsing = false;
                    }
                }
            }
        }
        ImGui::End();
        ImGui::PopStyleVar();

        // 7. Tilemap Painting Panel
        ImGui::Begin("Tilemap Painting");
        if (m_SelectedEntity && m_SelectedEntity.HasComponent<PixelEngine::TilemapComponent>()) {
            auto& tc = m_SelectedEntity.GetComponent<PixelEngine::TilemapComponent>();
            
            // List tileset assets in project
            ImGui::Text("Available Tilesets:");
            for (const auto& [uuid, meta] : PixelEngine::AssetManager::GetMetadataRegistry()) {
                if (meta.Type == PixelEngine::AssetType::Tileset) {
                    bool isSelected = (tc.TilesetID == uuid);
                    if (ImGui::Selectable(meta.SourcePath.c_str(), isSelected)) {
                        tc.TilesetID = uuid;
                    }
                }
            }

            ImGui::Separator();
            auto tileset = PixelEngine::AssetManager::GetTileset(tc.TilesetID);
            
            if (tileset) {
                // Brush selection
                bool isPaint = (m_BrushType == BrushType::Paint);
                bool isErase = (m_BrushType == BrushType::Erase);
                
                if (ImGui::RadioButton(ICON_FA_PAINT_BRUSH " Paint Brush", isPaint)) {
                    m_BrushType = BrushType::Paint;
                }
                ImGui::SameLine();
                if (ImGui::RadioButton(ICON_FA_ERASER " Eraser", isErase)) {
                    m_BrushType = BrushType::Erase;
                }
                
                ImGui::Separator();
                ImGui::Text("Palette Grid");
                
                auto texture = PixelEngine::AssetManager::GetTexture(tileset->TextureID);
                if (texture) {
                    VkDescriptorSet textureDS = PixelEngine::AssetManager::GetTextureDescriptorSet(tileset->TextureID);
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
                            
                            m_SelectedTileIndex = row * cols + col + 1; // 1-based index
                        }
                        
                        // Draw highlight on selected tile
                        if (m_SelectedTileIndex > 0) {
                            uint32_t cols = texture->GetWidth() / tileset->TileSize;
                            uint32_t col = (m_SelectedTileIndex - 1) % cols;
                            uint32_t row = (m_SelectedTileIndex - 1) / cols;
                            
                            ImVec2 tileStart(startPos.x + col * tileset->TileSize * scale, startPos.y + row * tileset->TileSize * scale);
                            ImVec2 tileEnd(tileStart.x + tileset->TileSize * scale, tileStart.y + tileset->TileSize * scale);
                            
                            ImGui::GetWindowDrawList()->AddRect(tileStart, tileEnd, IM_COL32(255, 255, 0, 255), 0.0f, 0, 2.0f);
                        }
                    }
                } else {
                    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Tileset texture not loaded.");
                }
                
                ImGui::Separator();
                if (m_SelectedTileIndex > 0) {
                    ImGui::Text("Selected Tile Index: %u", m_SelectedTileIndex);
                    bool isSolid = tileset->SolidTiles[m_SelectedTileIndex];
                    if (ImGui::Checkbox("Is Solid Tile (Collision)", &isSolid)) {
                        tileset->SolidTiles[m_SelectedTileIndex] = isSolid;
                        // Save changes to disk
                        std::string tilesetPath = PixelEngine::AssetManager::GetAssetPath(tileset->ID);
                        std::ofstream fout(tilesetPath);
                        if (fout.is_open()) {
                            nlohmann::json tsJson;
                            tsJson["tileSize"] = tileset->TileSize;
                            std::string texRelPath = "";
                            auto texMetadata = PixelEngine::AssetManager::GetMetadataRegistry().find(tileset->TextureID);
                            if (texMetadata != PixelEngine::AssetManager::GetMetadataRegistry().end()) {
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

        // 8. Animation Editor Panel
        ImGui::Begin("Animation Editor");
        if (m_SelectedEntity && m_SelectedEntity.HasComponent<PixelEngine::AnimatorComponent>()) {
            auto& ac = m_SelectedEntity.GetComponent<PixelEngine::AnimatorComponent>();
            
            // 1. Spritesheet selection
            ImGui::Text("Select Sprite Sheet:");
            for (const auto& [uuid, meta] : PixelEngine::AssetManager::GetMetadataRegistry()) {
                if (meta.Type == PixelEngine::AssetType::SpriteSheet) {
                    bool isSelected = (ac.SpriteSheetID == uuid);
                    if (ImGui::Selectable(meta.SourcePath.c_str(), isSelected)) {
                        ac.SpriteSheetID = uuid;
                    }
                }
            }
            
            ImGui::Separator();
            
            auto spritesheet = PixelEngine::AssetManager::GetSpriteSheet(ac.SpriteSheetID);
            if (spritesheet) {
                // 2. Clip controls
                ImGui::Text("Clips:");
                static char newClipBuffer[64] = "";
                ImGui::InputText("New Clip Name", newClipBuffer, sizeof(newClipBuffer));
                ImGui::SameLine();
                if (ImGui::Button("Add Clip") && strlen(newClipBuffer) > 0) {
                    PixelEngine::AnimationClip newClip;
                    newClip.Name = newClipBuffer;
                    newClip.FPS = 10.0f;
                    newClip.Loop = true;
                    ac.Clips.push_back(newClip);
                    if (ac.CurrentClip.empty()) {
                        ac.CurrentClip = newClip.Name;
                    }
                    newClipBuffer[0] = '\0';
                }
                
                int clipToDelete = -1;
                for (int i = 0; i < static_cast<int>(ac.Clips.size()); i++) {
                    auto& clip = ac.Clips[i];
                    bool isCurrent = (ac.CurrentClip == clip.Name);
                    
                    ImGui::PushID(i);
                    if (ImGui::Selectable(clip.Name.c_str(), isCurrent)) {
                        ac.CurrentClip = clip.Name;
                        ac.CurrentFrame = 0;
                        ac.Timer = 0.0f;
                    }
                    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 60.0f);
                    if (ImGui::Button("Delete")) {
                        clipToDelete = i;
                    }
                    ImGui::PopID();
                }
                
                if (clipToDelete >= 0) {
                    std::string deletedName = ac.Clips[clipToDelete].Name;
                    ac.Clips.erase(ac.Clips.begin() + clipToDelete);
                    if (ac.CurrentClip == deletedName) {
                        ac.CurrentClip = ac.Clips.empty() ? "" : ac.Clips[0].Name;
                        ac.CurrentFrame = 0;
                        ac.Timer = 0.0f;
                    }
                }
                
                ImGui::Separator();
                
                // 3. Edit current clip
                PixelEngine::AnimationClip* currentClip = nullptr;
                for (auto& c : ac.Clips) {
                    if (c.Name == ac.CurrentClip) {
                        currentClip = &c;
                        break;
                    }
                }
                
                if (currentClip) {
                    ImGui::Text("Editing Clip: %s", currentClip->Name.c_str());
                    
                    ImGui::SliderFloat("FPS", &currentClip->FPS, 1.0f, 60.0f);
                    ImGui::Checkbox("Loop", &currentClip->Loop);
                    
                    // Display preview/playback controls
                    ImGui::Checkbox("Play Preview", &ac.Playing);
                    
                    ImGui::Text("Timeline Frames (Count: %d):", (int)currentClip->Frames.size());
                    
                    // List frames in current clip
                    int frameToMoveUp = -1;
                    int frameToMoveDown = -1;
                    int frameToRemove = -1;
                    
                    for (int i = 0; i < static_cast<int>(currentClip->Frames.size()); i++) {
                        auto& frame = currentClip->Frames[i];
                        ImGui::PushID(i);
                        
                        ImGui::Text("[%d] Frame: %s", i, frame.FrameName.c_str());
                        ImGui::SameLine();
                        
                        char eventBuf[64];
                        strncpy_s(eventBuf, frame.EventName.c_str(), sizeof(eventBuf));
                        ImGui::SetNextItemWidth(100.0f);
                        if (ImGui::InputText("Event", eventBuf, sizeof(eventBuf))) {
                            frame.EventName = eventBuf;
                        }
                        
                        ImGui::SameLine();
                        if (ImGui::Button("^") && i > 0) {
                            frameToMoveUp = i;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("v") && i < static_cast<int>(currentClip->Frames.size()) - 1) {
                            frameToMoveDown = i;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("X")) {
                            frameToRemove = i;
                        }
                        
                        ImGui::PopID();
                    }
                    
                    if (frameToMoveUp >= 0) {
                        std::swap(currentClip->Frames[frameToMoveUp], currentClip->Frames[frameToMoveUp - 1]);
                    }
                    if (frameToMoveDown >= 0) {
                        std::swap(currentClip->Frames[frameToMoveDown], currentClip->Frames[frameToMoveDown + 1]);
                    }
                    if (frameToRemove >= 0) {
                        currentClip->Frames.erase(currentClip->Frames.begin() + frameToRemove);
                    }
                    
                    ImGui::Separator();
                    ImGui::Text("Add Frame from Sprite Sheet:");
                    
                    // Show available frames from the spritesheet
                    for (const auto& [frameName, frameData] : spritesheet->Frames) {
                        if (ImGui::Button(frameName.c_str())) {
                            PixelEngine::AnimationFrame newFrame;
                            newFrame.FrameName = frameName;
                            newFrame.EventName = "";
                            currentClip->Frames.push_back(newFrame);
                        }
                    }
                }
            } else {
                ImGui::Text("No Sprite Sheet loaded on this animator.");
            }
        } else {
            ImGui::Text("Select an entity with an Animator Component to edit animations.");
        }
        ImGui::End();

        // 9. Audio Mixer Panel
        ImGui::Begin("Audio Mixer");
        {
            float masterVol = PixelEngine::AudioManager::GetMasterVolume();
            if (ImGui::SliderFloat("Master Volume", &masterVol, 0.0f, 1.0f)) {
                PixelEngine::AudioManager::SetMasterVolume(masterVol);
            }

            float musicVol = PixelEngine::AudioManager::GetMusicVolume();
            if (ImGui::SliderFloat("Music Volume", &musicVol, 0.0f, 1.0f)) {
                PixelEngine::AudioManager::SetMusicVolume(musicVol);
            }

            float sfxVol = PixelEngine::AudioManager::GetSFXVolume();
            if (ImGui::SliderFloat("SFX Volume", &sfxVol, 0.0f, 1.0f)) {
                PixelEngine::AudioManager::SetSFXVolume(sfxVol);
            }
        }
        ImGui::End();

        // End Dockspace window
        ImGui::End();

        // Update Camera Projection Matrix
        float aspect = m_OffscreenTarget->GetWidth() / (float)m_OffscreenTarget->GetHeight();
        m_Camera.SetPerspectiveProjection(glm::radians(45.0f), aspect, 0.1f, 100.0f);


    }

    void OnRender() override {
        uint32_t imageIndex = GetCurrentImageIndex();
        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        auto& context = GetVulkanContext();

        // Pass 1: Offscreen ECS Render (only if project is loaded)
        if (m_ProjectLoaded) {
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = context.GetOffscreenRenderPass();
            renderPassInfo.framebuffer = m_OffscreenTarget->GetFramebuffer();
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = { m_OffscreenTarget->GetWidth(), m_OffscreenTarget->GetHeight() };

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {{0.01f, 0.01f, 0.01f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};

            renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = (float)m_OffscreenTarget->GetWidth();
            viewport.height = (float)m_OffscreenTarget->GetHeight();
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = { m_OffscreenTarget->GetWidth(), m_OffscreenTarget->GetHeight() };
            vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

            // Delegate rendering to ECS system
            m_RenderSystem->Render(commandBuffer, imageIndex, *m_ActiveScene, m_Camera);

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

            if (m_ProjectLoaded) {
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

    bool MatchAssetFilter(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        std::string filename = path.filename().string();
        if (ext == ".meta" || filename == "asset_registry.json") return false;

        switch (m_AssetFilter) {
            case AssetFilter::Textures: return (ext == ".png" || ext == ".jpg" || ext == ".tga");
            case AssetFilter::Audio: return (ext == ".wav" || ext == ".mp3" || ext == ".ogg");
            case AssetFilter::Prefabs: return (ext == ".prefab" || filename.ends_with(".prefab.json"));
            case AssetFilter::Scenes: return (ext == ".json" && !filename.ends_with(".prefab.json"));
            default: return true;
        }
    }

    bool MatchAssetSearch(const std::filesystem::path& path, const std::string& query) {
        if (query.empty()) return true;
        std::string filename = path.filename().string();
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        return filename.find(query) != std::string::npos;
    }

    void DrawDirectoryGridItem(const std::filesystem::path& path, const std::filesystem::path& assetRoot) {
        auto filename = path.filename().string();
        ImGui::BeginGroup();
        
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.2f, 0.3f, 1.0f));
        if (ImGui::Button((std::string(ICON_FA_FOLDER "\n\n") + filename).c_str(), ImVec2(80, 80))) {
            // Navigation happens on double click
        }
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            m_CurrentDirectory = path;
        }

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID")) {
                PixelEngine::UUID entityUUID = *(const PixelEngine::UUID*)payload->Data;
                SaveEntityAsPrefab(entityUUID, path);
            }
            ImGui::EndDragDropTarget();
        }

        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 80.0f);
        ImGui::Text("%s", filename.c_str());
        ImGui::PopTextWrapPos();

        ImGui::EndGroup();
    }

    void DrawAssetGridItem(const std::filesystem::path& path, const std::filesystem::path& assetRoot) {
        auto filename = path.filename().string();
        std::string ext = path.extension().string();
        ImGui::BeginGroup();

        bool drawn = false;
        if (ext == ".png" || ext == ".jpg" || ext == ".tga") {
            std::string relativePath = std::filesystem::relative(path, assetRoot).string();
            PixelEngine::UUID uuid = PixelEngine::AssetManager::LoadTexture(relativePath);
            if (uuid != 0) {
                VkDescriptorSet descriptorSet = PixelEngine::AssetManager::GetTextureDescriptorSet(uuid);
                if (descriptorSet != VK_NULL_HANDLE) {
                    ImGui::Image((ImTextureID)descriptorSet, ImVec2(80, 80));
                    drawn = true;
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
            }
            ImGui::Button((iconLabel + "\n" + filename).c_str(), ImVec2(80, 80));
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

    bool EntityMatchesFilters(PixelEngine::Entity entity, const std::string& searchQuery) {
        if (!searchQuery.empty()) {
            std::string tag = entity.GetComponent<PixelEngine::TagComponent>().Tag;
            std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
            if (tag.find(searchQuery) == std::string::npos) {
                return false;
            }
        }

        switch (m_HierarchyFilter) {
            case HierarchyFilter::Script: return entity.HasComponent<PixelEngine::ScriptComponent>();
            case HierarchyFilter::Sprite: return entity.HasComponent<PixelEngine::SpriteRendererComponent>();
            case HierarchyFilter::Audio: return entity.HasComponent<PixelEngine::AudioSourceComponent>();
            case HierarchyFilter::Tilemap: return entity.HasComponent<PixelEngine::TilemapComponent>();
            default: return true;
        }
    }

    void DrawEntityNode(PixelEngine::Entity entity) {
        auto& tag = entity.GetComponent<PixelEngine::TagComponent>().Tag;
        auto myUUID = entity.GetComponent<PixelEngine::IDComponent>().ID;
        
        ImGuiTreeNodeFlags flags = ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
        
        bool hasChildren = false;
        if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
            hasChildren = !entity.GetComponent<PixelEngine::HierarchyComponent>().Children.empty();
        }
        
        if (!hasChildren) {
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }
        
        std::string icon = "";
        if (entity.HasComponent<PixelEngine::AudioSourceComponent>()) {
            icon = ICON_FA_VOLUME_HIGH " ";
        } else if (entity.HasComponent<PixelEngine::TilemapComponent>()) {
            icon = ICON_FA_MAP " ";
        } else if (entity.HasComponent<PixelEngine::AnimatorComponent>()) {
            icon = ICON_FA_FILM " ";
        } else if (entity.HasComponent<PixelEngine::SpriteRendererComponent>()) {
            icon = ICON_FA_IMAGE " ";
        } else if (entity.HasComponent<PixelEngine::MeshRendererComponent>()) {
            icon = ICON_FA_CUBES " ";
        } else {
            icon = ICON_FA_GEAR " ";
        }
        
        std::string label = icon + tag;
        
        PixelEngine::UUID prefabID = 0;
        bool isPrefab = IsPartOfPrefab(entity, prefabID);
        if (isPrefab) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.376f, 0.647f, 0.980f, 1.0f));
        }
        
        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)myUUID, flags, "%s", label.c_str());
        
        if (isPrefab) {
            ImGui::PopStyleColor();
        }
        
        if (ImGui::IsItemClicked()) {
            m_SelectedEntity = entity;
        }
        
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Entity")) {
                PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                nlohmann::json beforeState = serializer.SerializeToJson();
                
                m_ActiveScene->DestroyEntity(entity);
                if (m_SelectedEntity == entity) m_SelectedEntity = {};
                
                nlohmann::json afterState = serializer.SerializeToJson();
                PixelEngine::EditorHistory::PushCommand(
                    std::make_unique<SceneSnapshotCommand>(m_ActiveScene, beforeState, afterState, "Delete Entity")
                );
            }
            ImGui::EndPopup();
        }
        
        if (ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("ENTITY_UUID", &myUUID, sizeof(PixelEngine::UUID));
            ImGui::Text("Dragging %s", tag.c_str());
            ImGui::EndDragDropSource();
        }
        
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID")) {
                PixelEngine::UUID draggedUUID = *(const PixelEngine::UUID*)payload->Data;
                if (draggedUUID != myUUID) {
                    auto draggedEntity = m_ActiveScene->GetEntityByUUID(draggedUUID);
                    if (draggedEntity) {
                        // Reparent
                        if (draggedEntity.HasComponent<PixelEngine::HierarchyComponent>()) {
                            auto& draggedHc = draggedEntity.GetComponent<PixelEngine::HierarchyComponent>();
                            if (draggedHc.Parent != 0) {
                                auto oldParent = m_ActiveScene->GetEntityByUUID(draggedHc.Parent);
                                if (oldParent && oldParent.HasComponent<PixelEngine::HierarchyComponent>()) {
                                    auto& oldParentHc = oldParent.GetComponent<PixelEngine::HierarchyComponent>();
                                    oldParentHc.Children.erase(std::remove(oldParentHc.Children.begin(), oldParentHc.Children.end(), draggedUUID), oldParentHc.Children.end());
                                }
                            }
                        } else {
                            draggedEntity.AddComponent<PixelEngine::HierarchyComponent>();
                        }
                        
                        auto& draggedHc = draggedEntity.GetComponent<PixelEngine::HierarchyComponent>();
                        draggedHc.Parent = myUUID;
                        
                        if (!entity.HasComponent<PixelEngine::HierarchyComponent>()) {
                            entity.AddComponent<PixelEngine::HierarchyComponent>();
                        }
                        auto& ourHc = entity.GetComponent<PixelEngine::HierarchyComponent>();
                        if (std::find(ourHc.Children.begin(), ourHc.Children.end(), draggedUUID) == ourHc.Children.end()) {
                            ourHc.Children.push_back(draggedUUID);
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }
        
        if (opened && hasChildren) {
            auto& hc = entity.GetComponent<PixelEngine::HierarchyComponent>();
            for (auto childUUID : hc.Children) {
                auto childEntity = m_ActiveScene->GetEntityByUUID(childUUID);
                if (childEntity) {
                    DrawEntityNode(childEntity);
                }
            }
            ImGui::TreePop();
        }
    }

    void TrackOverride(PixelEngine::Entity entity, const std::string& fieldName) {
        if (!entity || !entity.HasComponent<PixelEngine::PrefabComponent>()) return;
        auto& pc = entity.GetComponent<PixelEngine::PrefabComponent>();
        if (std::find(pc.OverriddenFields.begin(), pc.OverriddenFields.end(), fieldName) == pc.OverriddenFields.end()) {
            pc.OverriddenFields.push_back(fieldName);
        }
    }

    bool IsPartOfPrefab(PixelEngine::Entity entity, PixelEngine::UUID& outPrefabID) {
        if (!entity) return false;
        if (entity.HasComponent<PixelEngine::PrefabComponent>()) {
            outPrefabID = entity.GetComponent<PixelEngine::PrefabComponent>().PrefabID;
            return true;
        }
        if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
            auto parentUUID = entity.GetComponent<PixelEngine::HierarchyComponent>().Parent;
            if (parentUUID != 0) {
                auto parentEnt = m_ActiveScene->GetEntityByUUID(parentUUID);
                return IsPartOfPrefab(parentEnt, outPrefabID);
            }
        }
        return false;
    }

    std::filesystem::path FindPrefabPath(PixelEngine::UUID prefabID) {
        std::filesystem::path assetsPath = m_ProjectLoaded ? (std::filesystem::path(m_ProjectPath) / "assets") : std::filesystem::path("assets");
        if (std::filesystem::exists(assetsPath)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)) {
                if (entry.is_regular_file() && (entry.path().extension() == ".json" && entry.path().string().find(".prefab") != std::string::npos)) {
                    std::ifstream fin(entry.path());
                    if (fin.is_open()) {
                        try {
                            nlohmann::json j;
                            fin >> j;
                            if (j.contains("PrefabID") && j["PrefabID"].get<uint64_t>() == static_cast<uint64_t>(prefabID)) {
                                return entry.path();
                            }
                        } catch (...) {}
                    }
                }
            }
        }
        return {};
    }

    static glm::vec3 DeserializeVec3(const nlohmann::json& j) {
        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }

    static glm::vec4 DeserializeVec4(const nlohmann::json& j) {
        return glm::vec4(j[0].get<float>(), j[1].get<float>(), j[2].get<float>(), j[3].get<float>());
    }

    static nlohmann::json SerializeVec3(const glm::vec3& v) {
        return {v.x, v.y, v.z};
    }

    static nlohmann::json SerializeVec4(const glm::vec4& v) {
        return {v.x, v.y, v.z, v.w};
    }

    void SaveEntityAsPrefab(PixelEngine::UUID entityUUID, const std::filesystem::path& folderPath) {
        auto entity = m_ActiveScene->GetEntityByUUID(entityUUID);
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
                    auto childEnt = m_ActiveScene->GetEntityByUUID(childUUID);
                    if (childEnt) {
                        collectAndTagDescendants(childEnt);
                    }
                }
            }
        };
        collectAndTagDescendants(entity);

        PixelEngine::SceneSerializer serializer(*m_ActiveScene);
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

        PixelEngine::SceneSerializer serializer(*m_ActiveScene);
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
            PixelEngine::Entity newEntity = m_ActiveScene->CreateEntityWithUUID(newUUID, name);

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
            std::make_unique<SceneSnapshotCommand>(m_ActiveScene, beforeState, afterState, "Instantiate Prefab")
        );

        m_SelectedEntity = rootEntity;
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
        std::filesystem::path prefabPath = FindPrefabPath(prefabID);
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
                    auto childEnt = m_ActiveScene->GetEntityByUUID(childUUID);
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
        auto view = m_ActiveScene->Reg().view<PixelEngine::PrefabComponent>();
        std::vector<PixelEngine::Entity> roots;

        for (auto entityID : view) {
            PixelEngine::Entity entity = { entityID, m_ActiveScene.get() };
            if (entity.GetComponent<PixelEngine::PrefabComponent>().PrefabID == prefabID) {
                bool isRootInstance = true;
                if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
                    auto parentUUID = entity.GetComponent<PixelEngine::HierarchyComponent>().Parent;
                    if (parentUUID != 0) {
                        auto parentEnt = m_ActiveScene->GetEntityByUUID(parentUUID);
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

    void ApplyPrefabOverrides(PixelEngine::Entity instanceRoot, PixelEngine::UUID prefabID) {
        std::filesystem::path prefabPath = FindPrefabPath(prefabID);
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
                    auto childEnt = m_ActiveScene->GetEntityByUUID(childUUID);
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

        PixelEngine::SceneSerializer serializer(*m_ActiveScene);
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
                    auto childEnt = m_ActiveScene->GetEntityByUUID(childUUID);
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

    void DrawDirectoryNodes(const std::filesystem::path& dirPath) {
        if (!std::filesystem::exists(dirPath)) return;
        
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            auto path = entry.path();
            auto filename = path.filename().string();
            
            if (entry.is_directory()) {
                std::string label = std::string(ICON_FA_FOLDER " ") + filename;
                bool node_open = ImGui::TreeNode(label.c_str());
                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_UUID")) {
                        PixelEngine::UUID entityUUID = *(const PixelEngine::UUID*)payload->Data;
                        SaveEntityAsPrefab(entityUUID, path);
                    }
                    ImGui::EndDragDropTarget();
                }
                if (node_open) {
                    DrawDirectoryNodes(path);
                    ImGui::TreePop();
                }
            } else {
                std::string ext = path.extension().string();
                if (ext == ".meta" || filename == "asset_registry.json") continue;
                
                std::string label = std::string(ICON_FA_FILE " ") + filename;
                ImGui::Text("%s", label.c_str());
                if (ImGui::BeginDragDropSource()) {
                    std::string pathString = path.string();
                    ImGui::SetDragDropPayload("ASSET_PATH", pathString.c_str(), pathString.size() + 1);
                    ImGui::Text("Dragging %s", filename.c_str());
                    ImGui::EndDragDropSource();
                }
            }
        }
    }

    std::filesystem::path GetExecutableDirectory() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        return std::filesystem::path(path).parent_path();
    }

public:
    void LoadProject(const std::string& projectPathStr) {
        auto& context = GetVulkanContext();
        
        m_ProjectPath = projectPathStr;
        std::filesystem::path projectPath(m_ProjectPath);
        m_ProjectName = projectPath.filename().string();
        
        // 1. Set up Assets Root
        std::filesystem::path assetsPath = projectPath / "assets";
        std::filesystem::create_directories(assetsPath);
        PixelEngine::AssetManager::Init(context, assetsPath.string());
        m_CurrentDirectory = assetsPath;

        // 2. Set up shaders
        std::filesystem::path shadersPath = projectPath / "shaders";
        if (!std::filesystem::exists(shadersPath)) {
            shadersPath = std::filesystem::absolute("shaders");
        }
        PixelEngine::ShaderHotReloader::Init(shadersPath.string());
        
        // 3. Set up Asset Watcher
        PixelEngine::AssetWatcher::Init(assetsPath.string());

        // 4. Initialize Scene
        m_EditorScene = std::make_shared<PixelEngine::Scene>();
        m_ActiveScene = m_EditorScene;
        m_SelectedEntity = {};
        
        // 5. Load Startup Scene if it exists
        std::filesystem::path projFilePath = projectPath / (m_ProjectName + ".pixelproj");
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
            m_ActiveScenePath = startupScenePath;
            PixelEngine::SceneSerializer serializer(*m_ActiveScene);
            serializer.Deserialize((projectPath / startupScenePath).string());
        } else {
            m_ActiveScenePath = "assets/scenes/startup.json";
            
            // Generate default scene assets and entity
            testTexture = 0;
            std::filesystem::path defaultPng = projectPath / "assets/test.png";
            if (std::filesystem::exists(defaultPng)) {
                testTexture = PixelEngine::AssetManager::LoadTexture(defaultPng.string());
            }

            // Textured Cube Entity
            auto cube = m_EditorScene->CreateEntity("Textured Cube");
            cube.AddComponent<PixelEngine::MeshRendererComponent>();
            cube.GetComponent<PixelEngine::MeshRendererComponent>().TextureID = testTexture;

            // Floating Sprite Entity
            auto sprite = m_EditorScene->CreateEntity("Floating Sprite");
            sprite.AddComponent<PixelEngine::SpriteRendererComponent>();
            sprite.GetComponent<PixelEngine::SpriteRendererComponent>().Mat.TextureID = testTexture;
            sprite.GetComponent<PixelEngine::TransformComponent>().Translation = {2.0f, 0.0f, 0.0f};
        }

        // 6. Initialize ScriptEngine
        std::filesystem::path coreAssemblyPath = GetExecutableDirectory() / "PixelEngineScripting.dll";
        PixelEngine::ScriptEngine::Init(coreAssemblyPath.string());
        PixelEngine::ScriptEngine::SetActiveScene(m_ActiveScene);

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
        auto scriptView = m_ActiveScene->Reg().view<PixelEngine::IDComponent>();
        for (auto entityID : scriptView) {
            PixelEngine::Entity entity = { entityID, m_ActiveScene.get() };
            if (entity.HasComponent<PixelEngine::ScriptComponent>()) {
                PixelEngine::ScriptEngine::OnCreateEntity(entity);
            }
        }

        m_ProjectLoaded = true;
        
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

public class Player : ScriptableEntity {
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
        m_RecentProjects.clear();
        std::string configPath = "recent_projects.json";
        if (std::filesystem::exists(configPath)) {
            std::ifstream fin(configPath);
            if (fin.is_open()) {
                try {
                    nlohmann::json configJson;
                    fin >> configJson;
                    if (configJson.is_array()) {
                        for (const auto& item : configJson) {
                            m_RecentProjects.push_back(item.get<std::string>());
                        }
                    }
                } catch (...) {}
                fin.close();
            }
        }
    }

    void AddToRecentProjects(const std::string& path) {
        m_RecentProjects.erase(std::remove(m_RecentProjects.begin(), m_RecentProjects.end(), path), m_RecentProjects.end());
        m_RecentProjects.insert(m_RecentProjects.begin(), path);
        if (m_RecentProjects.size() > 5) {
            m_RecentProjects.resize(5);
        }
        std::string configPath = "recent_projects.json";
        std::ofstream fout(configPath);
        if (fout.is_open()) {
            nlohmann::json configJson = m_RecentProjects;
            fout << configJson.dump(4) << std::endl;
            fout.close();
        }
    }

    void DrawLauncher() {
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
        
        if (m_RecentProjects.empty()) {
            ImGui::Dummy(ImVec2(0, 40.0f));
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No recent projects found.");
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "Create or open a project to get started!");
        } else {
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.0f));
            
            for (size_t i = 0; i < m_RecentProjects.size(); ++i) {
                const auto& proj = m_RecentProjects[i];
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
                
                if (clicked) {
                    LoadProject(proj);
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
                static char projName[128] = "MyNewProject";
                
                if (parentFolder[0] == '\0') {
                    std::filesystem::path defaultPath = std::filesystem::current_path() / "projects";
                    strcpy_s(parentFolder, defaultPath.string().c_str());
                }
                
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Project Name");
                ImGui::InputText("##CreateProjName", projName, sizeof(projName));
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
                
                if (ImGui::Button(ICON_FA_PLUS "  Create New Project", ImVec2(-1, 48))) {
                    CreateNewProject(parentFolder, projName);
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
                
                if (ImGui::Button(ICON_FA_FOLDER "  Open Existing Project", ImVec2(-1, 48))) {
                    std::filesystem::path path(openProjPath);
                    if (std::filesystem::exists(path)) {
                        if (path.extension() == ".pixelproj") {
                            LoadProject(path.parent_path().string());
                        } else if (std::filesystem::is_directory(path)) {
                            LoadProject(path.string());
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

    bool OnCloseRequested() override {
        if (m_ProjectLoaded && PixelEngine::EditorHistory::IsDirty()) {
            m_ShowExitPopup = true;
            return false; // Intercept close
        }
        return true; // Safe to close
    }

    void DrawExitPopup() {
        if (m_ShowExitPopup) {
            ImGui::OpenPopup("Save Changes?");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        
        if (ImGui::BeginPopupModal("Save Changes?", &m_ShowExitPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes. Do you want to save before exiting?");
            ImGui::Separator();

            if (ImGui::Button(ICON_FA_SAVE " Save", ImVec2(120, 0))) {
                std::filesystem::path projectPath(m_ProjectPath);
                std::filesystem::path scenePath = projectPath / (m_ActiveScenePath.empty() ? "assets/scenes/startup.json" : m_ActiveScenePath);
                PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                serializer.Serialize(scenePath.string());
                PixelEngine::EditorHistory::SetDirty(false);
                
                m_ShowExitPopup = false;
                Close();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
                m_ShowExitPopup = false;
                PixelEngine::EditorHistory::SetDirty(false);
                Close();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_ShowExitPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void DrawCloseProjectPopup() {
        if (m_ShowCloseProjectPopup) {
            ImGui::OpenPopup("Save Project Changes?");
        }

        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        
        if (ImGui::BeginPopupModal("Save Project Changes?", &m_ShowCloseProjectPopup, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("You have unsaved changes. Do you want to save before closing the project?");
            ImGui::Separator();

            if (ImGui::Button(ICON_FA_SAVE " Save", ImVec2(120, 0))) {
                std::filesystem::path projectPath(m_ProjectPath);
                std::filesystem::path scenePath = projectPath / (m_ActiveScenePath.empty() ? "assets/scenes/startup.json" : m_ActiveScenePath);
                PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                serializer.Serialize(scenePath.string());
                PixelEngine::EditorHistory::SetDirty(false);
                
                m_ShowCloseProjectPopup = false;
                m_ProjectLoaded = false;
                m_ProjectPath = "";
                m_ProjectName = "";
                m_ActiveScenePath = "";
                SDL_SetWindowTitle(m_Window, "Pixel Editor Workspace");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save", ImVec2(120, 0))) {
                m_ShowCloseProjectPopup = false;
                PixelEngine::EditorHistory::SetDirty(false);
                m_ProjectLoaded = false;
                m_ProjectPath = "";
                m_ProjectName = "";
                m_ActiveScenePath = "";
                SDL_SetWindowTitle(m_Window, "Pixel Editor Workspace");
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                m_ShowCloseProjectPopup = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

private:
    std::unique_ptr<PixelEngine::GraphicsPipeline> m_UpscalePipeline;
    std::unique_ptr<PixelEngine::OffscreenTarget> m_OffscreenTarget;
    std::unique_ptr<PixelEngine::Buffer> m_QuadVertexBuffer;
    std::unique_ptr<PixelEngine::Buffer> m_QuadIndexBuffer;
    
    enum class SceneState { Edit = 0, Play = 1, Pause = 2 };
    SceneState m_SceneState = SceneState::Edit;

    std::shared_ptr<PixelEngine::Scene> m_EditorScene;
    std::shared_ptr<PixelEngine::Scene> m_ActiveScene;
    std::unique_ptr<PixelEngine::RenderSystem> m_RenderSystem;
    PixelEngine::Entity m_SelectedEntity;

    PixelEngine::Camera m_Camera;
    glm::vec2 m_CameraTarget = { 0.0f, 0.0f };
    float m_CameraDistance = 5.0f;
    float m_Rotation = 0.0f;

    VkDescriptorSet m_ViewportDescriptorSet = VK_NULL_HANDLE;
    ImVec2 m_ViewportSize = { 1280.0f, 720.0f };
    bool m_ViewportFocused = false;
    bool m_ViewportHovered = false;
    bool m_ResetLayout = false;
    int m_GizmoType = -1;

    bool m_PixelSnapping = true;
    float m_RotationSpeed = 50.0f;

    PixelEngine::UUID testTexture;

    enum class BrushType { Paint = 0, Erase = 1 };
    BrushType m_BrushType = BrushType::Paint;
    uint32_t m_SelectedTileIndex = 1;

    // Project Scoping Configuration
    bool m_ProjectLoaded = false;
    std::string m_ProjectPath = "";
    std::string m_ProjectName = "";
    std::string m_ActiveScenePath = "";
    std::vector<std::string> m_RecentProjects;

    // Undo/Redo & Modal configurations
    bool m_ShowExitPopup = false;
    bool m_ShowCloseProjectPopup = false;
    nlohmann::json m_BeforePaintState;
    bool m_IsPainting = false;

    char m_HierarchySearchBuffer[256] = "";
    enum class HierarchyFilter { All = 0, Script, Sprite, Audio, Tilemap };
    HierarchyFilter m_HierarchyFilter = HierarchyFilter::All;

    char m_AssetSearchBuffer[256] = "";
    enum class AssetFilter { All = 0, Textures, Audio, Prefabs, Scenes };
    AssetFilter m_AssetFilter = AssetFilter::All;
    std::filesystem::path m_CurrentDirectory;
};

int main(int argc, char* argv[]) {
    try {
        SandboxApp* app = new SandboxApp();
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
