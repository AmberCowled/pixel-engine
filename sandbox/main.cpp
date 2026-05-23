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
                ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
                ImGui::DockBuilderDockWindow("Profiler", dock_id_right);
                ImGui::DockBuilderDockWindow("Asset Browser", dock_id_bottom);
                ImGui::DockBuilderDockWindow("Console", dock_id_bottom);
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
