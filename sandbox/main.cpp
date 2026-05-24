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

class SandboxApp : public PixelEngine::EngineApp {
public:
    SandboxApp() : PixelEngine::EngineApp({"Pixel Editor Workspace", 1280, 720}) {
        PX_INFO("Sandbox Editor Workspace Started.");
        
        auto& context = GetVulkanContext();

        // 1. Initialize Asset Manager
        PixelEngine::AssetManager::Init(context);

        // 2. Initialize Renderer2D & ShaderHotReloader
        PixelEngine::Renderer2D::Init(context);
        PixelEngine::ShaderHotReloader::Init("shaders");
        PixelEngine::AssetWatcher::Init("assets");
        PixelEngine::AudioManager::Init();

        // 3. Create Offscreen Target (default size 1280x720 scaled by DPI)
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

        // 4. Link Offscreen Target to Upscale pipeline
        context.UpdateUpscaleDescriptorSets(m_OffscreenTarget->GetColorImageView());

        // 5. Create Geometry Buffers for Upscaling
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

        // 6. Create Upscale Pipeline
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

        PixelEngine::PipelineConfigInfo upscaleConfig{};
        PixelEngine::GraphicsPipeline::DefaultPipelineConfigInfo(upscaleConfig);
        upscaleConfig.renderPass = context.GetRenderPass();
        upscaleConfig.pipelineLayout = context.GetUpscalePipelineLayout();
        upscaleConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
        upscaleConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        upscaleConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        m_UpscalePipeline = std::make_unique<PixelEngine::GraphicsPipeline>(
            context, upscaleVert, upscaleFrag, upscaleConfig
        );

        // 7. Initialize ECS Systems
        m_EditorScene = std::make_shared<PixelEngine::Scene>();
        m_ActiveScene = m_EditorScene;
        m_RenderSystem = std::make_unique<PixelEngine::RenderSystem>(context);

        // 8. Load Assets & Create Entities
        testTexture = 0;
        std::vector<std::string> searchPaths = { "assets/test.png", "../assets/test.png", "../../assets/test.png", "../../../assets/test.png" };
        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path)) {
                testTexture = PixelEngine::AssetManager::LoadTexture(path);
                break;
            }
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
        // Start ImGuizmo frame
        ImGuizmo::BeginFrame();

        // Run Asset Watcher (automatically handles shaders and textures)
        PixelEngine::AssetWatcher::Update();

        // Update ECS Systems (Velocity, Animations, etc.) if Playing
        if (m_SceneState == SceneState::Play) {
            m_ActiveScene->OnUpdate(deltaTime);
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
                if (ImGui::MenuItem("Save Scene")) {
                    PixelEngine::SceneSerializer serializer(*m_ActiveScene);
                    serializer.Serialize("assets/scenes/sandbox_scene.json");
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
            float buttonSize = 32.0f;
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x * 0.5f - (buttonSize * 1.5f));
            
            // Play Button
            bool isPlay = (m_SceneState == SceneState::Play);
            if (ImGui::RadioButton("Play", isPlay)) {
                if (m_SceneState == SceneState::Edit) {
                    m_ActiveScene->StopAllAudio();
                    m_SceneState = SceneState::Play;
                    m_EditorScene = m_ActiveScene; // save original active scene
                    m_ActiveScene = PixelEngine::Scene::Clone(m_EditorScene);
                    m_SelectedEntity = {};
                } else if (m_SceneState == SceneState::Pause) {
                    m_SceneState = SceneState::Play;
                }
            }
            ImGui::SameLine();
            
            // Pause Button
            bool isPause = (m_SceneState == SceneState::Pause);
            if (ImGui::RadioButton("Pause", isPause)) {
                if (m_SceneState == SceneState::Play) {
                    m_SceneState = SceneState::Pause;
                }
            }
            ImGui::SameLine();
            
            // Stop Button
            if (ImGui::Button("Stop")) {
                if (m_SceneState == SceneState::Play || m_SceneState == SceneState::Pause) {
                    m_ActiveScene->StopAllAudio();
                    m_SceneState = SceneState::Edit;
                    m_ActiveScene = m_EditorScene; // restore edit scene
                    m_SelectedEntity = {};
                }
            }
        }
        ImGui::End();

        // 1. Hierarchy Window
        ImGui::Begin("Hierarchy");
        {
            auto idView = m_ActiveScene->Reg().view<PixelEngine::IDComponent>();
            for (auto ent : idView) {
                PixelEngine::Entity entity = { ent, m_ActiveScene.get() };
                bool isRoot = true;
                if (entity.HasComponent<PixelEngine::HierarchyComponent>()) {
                    isRoot = (entity.GetComponent<PixelEngine::HierarchyComponent>().Parent == 0);
                }
                if (isRoot) {
                    DrawEntityNode(entity);
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
                m_ActiveScene->CreateEntity("New Entity");
            }
        }
        ImGui::End();

        // 2. Inspector Window
        ImGui::Begin("Inspector");
        if (m_SelectedEntity) {
            auto& tag = m_SelectedEntity.GetComponent<PixelEngine::TagComponent>();
            char buffer[256];
            memset(buffer, 0, sizeof(buffer));
            strncpy_s(buffer, tag.Tag.c_str(), sizeof(buffer));
            if (ImGui::InputText("Tag", buffer, sizeof(buffer))) {
                tag.Tag = std::string(buffer);
            }

            ImGui::Separator();

            if (m_SelectedEntity.HasComponent<PixelEngine::TransformComponent>()) {
                if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& tc = m_SelectedEntity.GetComponent<PixelEngine::TransformComponent>();
                    ImGui::DragFloat3("Position", &tc.Translation.x, 0.1f);
                    ImGui::DragFloat3("Rotation", &tc.Rotation.x, 0.1f);
                    ImGui::DragFloat3("Scale", &tc.Scale.x, 0.1f);
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::MeshRendererComponent>()) {
                if (ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& mc = m_SelectedEntity.GetComponent<PixelEngine::MeshRendererComponent>();
                    ImGui::ColorEdit4("Mesh Color", &mc.Color.x);
                    
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
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::SpriteRendererComponent>()) {
                if (ImGui::CollapsingHeader("Sprite Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& sc = m_SelectedEntity.GetComponent<PixelEngine::SpriteRendererComponent>();
                    ImGui::ColorEdit4("Sprite Color", &sc.Mat.Color.x);

                    const char* blendModes[] = { "Opaque", "AlphaBlend", "Additive" };
                    int currentBlend = static_cast<int>(sc.Mat.Blend);
                    if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, 3)) {
                        sc.Mat.Blend = static_cast<PixelEngine::BlendMode>(currentBlend);
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
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::VelocityComponent>()) {
                if (ImGui::CollapsingHeader("Velocity", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& vc = m_SelectedEntity.GetComponent<PixelEngine::VelocityComponent>();
                    ImGui::DragFloat3("Linear", &vc.Linear.x, 0.05f);
                    ImGui::DragFloat3("Angular", &vc.Angular.x, 0.05f);
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
                    ImGui::Checkbox("Playing", &ac.Playing);
                    ImGui::Checkbox("Loop", &ac.Loop);
                    ImGui::SliderFloat("Frame Duration", &ac.FrameTime, 0.05f, 2.0f);
                    ImGui::Text("Frame Count: %d", (int)ac.Textures.size());
                    
                    if (ImGui::Button("Setup Test Anim (Blink)")) {
                        ac.Textures.clear();
                        ac.Textures.push_back(testTexture);
                        ac.Textures.push_back(0); // none texture ID
                        ac.CurrentFrame = 0;
                        ac.Timer = 0.0f;
                        ac.Playing = true;
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
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    int renderLayer = tc.RenderLayer;
                    if (ImGui::DragInt("Render Layer", &renderLayer, 1.0f)) {
                        tc.RenderLayer = renderLayer;
                    }

                    int tileSize = (int)tc.TileSize;
                    if (ImGui::DragInt("Tile Size", &tileSize, 1.0f, 1, 128)) {
                        tc.TileSize = static_cast<uint32_t>(tileSize);
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
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    ImGui::Checkbox("Playing", &ac.Playing);
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
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    // Properties
                    ImGui::Checkbox("Looping", &asc.Loop);
                    ImGui::Checkbox("Play On Start", &asc.PlayOnStart);
                    ImGui::Checkbox("Is Music Bus", &asc.IsMusic);

                    float volume = asc.Volume;
                    if (ImGui::SliderFloat("Volume", &volume, 0.0f, 1.0f)) {
                        asc.Volume = volume;
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

            ImGui::Separator();
            if (ImGui::Button("Add Component", ImVec2(-1, 30))) {
                ImGui::OpenPopup("AddComponentPopup");
            }
            
            if (ImGui::BeginPopup("AddComponentPopup")) {
                if (!m_SelectedEntity.HasComponent<PixelEngine::TransformComponent>() && ImGui::MenuItem("Transform")) {
                    m_SelectedEntity.AddComponent<PixelEngine::TransformComponent>();
                    ImGui::CloseCurrentPopup();
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::SpriteRendererComponent>() && ImGui::MenuItem("Sprite Renderer")) {
                    m_SelectedEntity.AddComponent<PixelEngine::SpriteRendererComponent>();
                    ImGui::CloseCurrentPopup();
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::MeshRendererComponent>() && ImGui::MenuItem("Mesh Renderer")) {
                    m_SelectedEntity.AddComponent<PixelEngine::MeshRendererComponent>();
                    ImGui::CloseCurrentPopup();
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::VelocityComponent>() && ImGui::MenuItem("Velocity")) {
                    m_SelectedEntity.AddComponent<PixelEngine::VelocityComponent>();
                    ImGui::CloseCurrentPopup();
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::HierarchyComponent>() && ImGui::MenuItem("Hierarchy")) {
                    m_SelectedEntity.AddComponent<PixelEngine::HierarchyComponent>();
                    ImGui::CloseCurrentPopup();
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::SpriteAnimationComponent>() && ImGui::MenuItem("Sprite Animation")) {
                    m_SelectedEntity.AddComponent<PixelEngine::SpriteAnimationComponent>();
                    ImGui::CloseCurrentPopup();
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::TilemapComponent>() && ImGui::MenuItem("Tilemap Component")) {
                    m_SelectedEntity.AddComponent<PixelEngine::TilemapComponent>();
                    ImGui::CloseCurrentPopup();
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::AnimatorComponent>() && ImGui::MenuItem("Animator Component")) {
                    m_SelectedEntity.AddComponent<PixelEngine::AnimatorComponent>();
                    ImGui::CloseCurrentPopup();
                }
                if (!m_SelectedEntity.HasComponent<PixelEngine::AudioSourceComponent>() && ImGui::MenuItem("Audio Source Component")) {
                    m_SelectedEntity.AddComponent<PixelEngine::AudioSourceComponent>();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();
            if (ImGui::Button("Delete Entity", ImVec2(-1, 30))) {
                m_ActiveScene->DestroyEntity(m_SelectedEntity);
                m_SelectedEntity = {};
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
            if (ImGui::TreeNode("assets")) {
                DrawDirectoryNodes("assets");
                ImGui::TreePop();
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

            ImVec2 imageMin = ImGui::GetItemRectMin();
            ImVec2 imageSize = ImGui::GetItemRectSize();

            if (m_SelectedEntity && m_SelectedEntity.HasComponent<PixelEngine::TilemapComponent>() && m_SceneState == SceneState::Edit) {
                if (m_ViewportHovered && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
                    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
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
                                    } else if (m_BrushType == BrushType::Erase) {
                                        tc.Chunks[chunkCoords].Tiles[localY * 16 + localX].TileIndex = 0;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Editor Orbit Camera Controls
            static float cameraPitch = 0.6f;
            static float cameraYaw = 0.8f;
            static float cameraDistance = 5.0f;
            
            if (m_ViewportHovered && !ImGuizmo::IsOver()) {
                cameraDistance -= io.MouseWheel * 0.25f;
                cameraDistance = std::max(cameraDistance, 1.0f);
                
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                    ImVec2 mouseDelta = io.MouseDelta;
                    cameraYaw -= mouseDelta.x * 0.005f;
                    cameraPitch += mouseDelta.y * 0.005f;
                    cameraPitch = glm::clamp(cameraPitch, -glm::half_pi<float>() + 0.1f, glm::half_pi<float>() - 0.1f);
                }
            }
            
            glm::vec3 cameraPos = glm::vec3(
                cameraDistance * cos(cameraPitch) * sin(cameraYaw),
                cameraDistance * sin(cameraPitch),
                cameraDistance * cos(cameraPitch) * cos(cameraYaw)
            );
            m_Camera.SetViewTarget(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f));

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
                
                if (ImGuizmo::IsUsing()) {
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
                const char* brushNames[] = { "Paint Brush", "Eraser" };
                int currentBrush = static_cast<int>(m_BrushType);
                if (ImGui::Combo("Brush Tool", &currentBrush, brushNames, 2)) {
                    m_BrushType = static_cast<BrushType>(currentBrush);
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

        // Pass 1: Offscreen ECS Render
        {
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

private:
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
        
        bool opened = ImGui::TreeNodeEx((void*)(uint64_t)myUUID, flags, "%s", tag.c_str());
        
        if (ImGui::IsItemClicked()) {
            m_SelectedEntity = entity;
        }
        
        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete Entity")) {
                m_ActiveScene->DestroyEntity(entity);
                if (m_SelectedEntity == entity) m_SelectedEntity = {};
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

    void DrawDirectoryNodes(const std::filesystem::path& dirPath) {
        if (!std::filesystem::exists(dirPath)) return;
        for (const auto& entry : std::filesystem::directory_iterator(dirPath)) {
            const auto& path = entry.path();
            std::string filename = path.filename().string();
            
            if (entry.is_directory()) {
                if (ImGui::TreeNode(filename.c_str())) {
                    DrawDirectoryNodes(path);
                    ImGui::TreePop();
                }
            } else {
                std::string ext = path.extension().string();
                if (ext == ".meta" || filename == "asset_registry.json") continue;
                
                ImGui::BulletText("%s", filename.c_str());
                if (ImGui::BeginDragDropSource()) {
                    std::string pathString = path.string();
                    ImGui::SetDragDropPayload("ASSET_PATH", pathString.c_str(), pathString.size() + 1);
                    ImGui::Text("Dragging %s", filename.c_str());
                    ImGui::EndDragDropSource();
                }
            }
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
