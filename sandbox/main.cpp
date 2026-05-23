#include <engine/core/EngineApp.hpp>
#include <engine/base/Log.hpp>
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
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <iostream>
#include <array>
#include <filesystem>

class SandboxApp : public PixelEngine::EngineApp {
public:
    SandboxApp() : PixelEngine::EngineApp({"Pixel Sandbox", 1280, 720}) {
        PX_INFO("Sandbox App Started.");
        
        auto& context = GetVulkanContext();

        // 1. Initialize Asset Manager
        PixelEngine::AssetManager::Init(context);

        // 2. Initialize Renderer2D & ShaderHotReloader
        PixelEngine::Renderer2D::Init(context);
        PixelEngine::ShaderHotReloader::Init("shaders");
        PixelEngine::AssetWatcher::Init("assets");

        // 3. Create Offscreen Target (320x180)
        PixelEngine::OffscreenTargetConfig offscreenConfig{};
        offscreenConfig.width = 320;
        offscreenConfig.height = 180;
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
        m_ActiveScene = std::make_unique<PixelEngine::Scene>();
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
        auto cube = m_ActiveScene->CreateEntity("Textured Cube");
        cube.AddComponent<PixelEngine::MeshRendererComponent>();
        cube.GetComponent<PixelEngine::MeshRendererComponent>().TextureID = testTexture;

        // Floating Sprite Entity
        auto sprite = m_ActiveScene->CreateEntity("Floating Sprite");
        sprite.AddComponent<PixelEngine::SpriteRendererComponent>();
        sprite.GetComponent<PixelEngine::SpriteRendererComponent>().Mat.TextureID = testTexture;
        sprite.GetComponent<PixelEngine::TransformComponent>().Translation = {2.0f, 0.0f, 0.0f};
    }

    ~SandboxApp() {
        PixelEngine::Renderer2D::Shutdown();
        PixelEngine::AssetManager::Shutdown();
        PX_INFO("Sandbox App Shutdown.");
    }

    void OnUpdate(float deltaTime) override {
        // Run Asset Watcher (automatically handles shaders and textures)
        PixelEngine::AssetWatcher::Update();

        // Update ECS Systems (Velocity, Animations, etc.)
        m_ActiveScene->OnUpdate(deltaTime);

        m_Rotation += deltaTime * m_RotationSpeed;

        auto& context = GetVulkanContext();
        
        // 1. Hierarchy Window
        ImGui::Begin("Hierarchy");
        auto view = m_ActiveScene->Reg().view<PixelEngine::TagComponent>();
        for (auto entity : view) {
            auto& tag = view.get<PixelEngine::TagComponent>(entity);
            bool isSelected = (m_SelectedEntity == entity);
            if (ImGui::Selectable(tag.Tag.c_str(), isSelected)) {
                m_SelectedEntity = {entity, m_ActiveScene.get()};
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Add Cube")) {
            auto e = m_ActiveScene->CreateEntity("New Cube");
            e.AddComponent<PixelEngine::MeshRendererComponent>();
        }
        if (ImGui::Button("Add Sprite")) {
            auto e = m_ActiveScene->CreateEntity("New Sprite");
            e.AddComponent<PixelEngine::SpriteRendererComponent>();
            e.GetComponent<PixelEngine::SpriteRendererComponent>().Mat.TextureID = testTexture;
        }
        if (ImGui::Button("Spawn 1000 Sprites")) {
            for (int i = 0; i < 1000; i++) {
                auto e = m_ActiveScene->CreateEntity("Stress Sprite " + std::to_string(i));
                e.AddComponent<PixelEngine::SpriteRendererComponent>();
                auto& tc = e.GetComponent<PixelEngine::TransformComponent>();
                tc.Translation = {
                    (rand() % 100 - 50) / 10.0f,
                    (rand() % 100 - 50) / 10.0f,
                    0.0f
                };
                tc.Scale = { 0.2f, 0.2f, 0.2f };
                // Randomize blend mode (0 = Opaque, 1 = AlphaBlend, 2 = Additive)
                e.GetComponent<PixelEngine::SpriteRendererComponent>().Mat.Blend = static_cast<PixelEngine::BlendMode>(rand() % 3);
                e.GetComponent<PixelEngine::SpriteRendererComponent>().Mat.TextureID = testTexture;
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
                auto& tc = m_SelectedEntity.GetComponent<PixelEngine::TransformComponent>();
                ImGui::DragFloat3("Position", &tc.Translation.x, 0.1f);
                ImGui::DragFloat3("Rotation", &tc.Rotation.x, 0.1f);
                ImGui::DragFloat3("Scale", &tc.Scale.x, 0.1f);
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::MeshRendererComponent>()) {
                auto& mc = m_SelectedEntity.GetComponent<PixelEngine::MeshRendererComponent>();
                ImGui::ColorEdit4("Mesh Color", &mc.Color.x);
            }

            if (m_SelectedEntity.HasComponent<PixelEngine::SpriteRendererComponent>()) {
                auto& sc = m_SelectedEntity.GetComponent<PixelEngine::SpriteRendererComponent>();
                ImGui::ColorEdit4("Sprite Color", &sc.Mat.Color.x);

                const char* blendModes[] = { "Opaque", "AlphaBlend", "Additive" };
                int currentBlend = static_cast<int>(sc.Mat.Blend);
                if (ImGui::Combo("Blend Mode", &currentBlend, blendModes, 3)) {
                    sc.Mat.Blend = static_cast<PixelEngine::BlendMode>(currentBlend);
                }
            }

            // --- Phase 3 Components UI ---
            
            // 1. Velocity Component
            ImGui::Separator();
            if (m_SelectedEntity.HasComponent<PixelEngine::VelocityComponent>()) {
                if (ImGui::TreeNodeEx("Velocity Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& vc = m_SelectedEntity.GetComponent<PixelEngine::VelocityComponent>();
                    ImGui::DragFloat3("Linear", &vc.Linear.x, 0.05f);
                    ImGui::DragFloat3("Angular", &vc.Angular.x, 0.05f);
                    if (ImGui::Button("Remove Velocity")) {
                        m_SelectedEntity.RemoveComponent<PixelEngine::VelocityComponent>();
                    }
                    ImGui::TreePop();
                }
            } else {
                if (ImGui::Button("Add Velocity Component")) {
                    m_SelectedEntity.AddComponent<PixelEngine::VelocityComponent>();
                }
            }

            // 2. Hierarchy Component
            ImGui::Separator();
            if (m_SelectedEntity.HasComponent<PixelEngine::HierarchyComponent>()) {
                if (ImGui::TreeNodeEx("Hierarchy Component", ImGuiTreeNodeFlags_DefaultOpen)) {
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

                    if (ImGui::BeginCombo("Select Parent", "Choose Parent...")) {
                        auto tagView = m_ActiveScene->Reg().view<PixelEngine::TagComponent, PixelEngine::IDComponent>();
                        for (auto ent : tagView) {
                            if (ent == m_SelectedEntity) continue;
                            auto& otherTag = tagView.get<PixelEngine::TagComponent>(ent);
                            auto otherUUID = tagView.get<PixelEngine::IDComponent>(ent).ID;
                            if (ImGui::Selectable(otherTag.Tag.c_str())) {
                                hc.Parent = otherUUID;
                                PixelEngine::Entity parentEnt = { ent, m_ActiveScene.get() };
                                if (!parentEnt.HasComponent<PixelEngine::HierarchyComponent>()) {
                                    parentEnt.AddComponent<PixelEngine::HierarchyComponent>();
                                }
                                auto& parentHc = parentEnt.GetComponent<PixelEngine::HierarchyComponent>();
                                auto myUUID = m_SelectedEntity.GetComponent<PixelEngine::IDComponent>().ID;
                                if (std::find(parentHc.Children.begin(), parentHc.Children.end(), myUUID) == parentHc.Children.end()) {
                                    parentHc.Children.push_back(myUUID);
                                }
                            }
                        }
                        ImGui::EndCombo();
                    }
                    ImGui::TreePop();
                }
            } else {
                if (ImGui::Button("Add Hierarchy Component")) {
                    m_SelectedEntity.AddComponent<PixelEngine::HierarchyComponent>();
                }
            }

            // 3. Sprite Animation Component
            ImGui::Separator();
            if (m_SelectedEntity.HasComponent<PixelEngine::SpriteAnimationComponent>()) {
                if (ImGui::TreeNodeEx("Sprite Animation Component", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto& ac = m_SelectedEntity.GetComponent<PixelEngine::SpriteAnimationComponent>();
                    ImGui::Checkbox("Playing", &ac.Playing);
                    ImGui::Checkbox("Loop", &ac.Loop);
                    ImGui::SliderFloat("Frame Duration", &ac.FrameTime, 0.05f, 2.0f);
                    ImGui::Text("Frame Count: %d", (int)ac.Textures.size());
                    
                    if (ImGui::Button("Setup Test Anim (Blink)")) {
                        ac.Textures.clear();
                        ac.Textures.push_back(testTexture);
                        ac.Textures.push_back(0); // white/none texture ID
                        ac.CurrentFrame = 0;
                        ac.Timer = 0.0f;
                        ac.Playing = true;
                    }
                    if (ImGui::Button("Remove Animation")) {
                        m_SelectedEntity.RemoveComponent<PixelEngine::SpriteAnimationComponent>();
                    }
                    ImGui::TreePop();
                }
            } else {
                if (ImGui::Button("Add Animation Component")) {
                    m_SelectedEntity.AddComponent<PixelEngine::SpriteAnimationComponent>();
                }
            }

            ImGui::Separator();

            if (ImGui::Button("Delete Entity")) {
                m_ActiveScene->DestroyEntity(m_SelectedEntity);
                m_SelectedEntity = {};
            }
        }
        ImGui::End();

        // 3. Engine Controls Window
        ImGui::Begin("Engine Controls");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("CPU Time: %.3f ms", deltaTime * 1000.0f);
        ImGui::Text("GPU Time: %.3f ms", context.GetGPUTime());
        
        // Display Renderer2D stats
        auto stats = PixelEngine::Renderer2D::GetStats();
        ImGui::Text("Batch Draw Calls: %u", stats.DrawCalls);
        ImGui::Text("Batch Quad Count: %u", stats.QuadCount);

        ImGui::Separator();

        bool resChanged = false;
        if (ImGui::SliderInt("Internal Width", &m_InternalWidth, 64, 1280)) resChanged = true;
        if (ImGui::SliderInt("Internal Height", &m_InternalHeight, 64, 720)) resChanged = true;
        
        if (resChanged) {
            vkDeviceWaitIdle(context.GetDevice());
            
            PixelEngine::OffscreenTargetConfig offscreenConfig{};
            offscreenConfig.width = (uint32_t)m_InternalWidth;
            offscreenConfig.height = (uint32_t)m_InternalHeight;
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
        }

        ImGui::Checkbox("Pixel Snapping", &m_PixelSnapping);
        ImGui::SliderFloat("Rotation Speed", &m_RotationSpeed, 0.0f, 200.0f);

        ImGui::Separator();
        if (ImGui::Button("Save Scene")) {
            PixelEngine::SceneSerializer serializer(*m_ActiveScene);
            serializer.Serialize("assets/scenes/sandbox_scene.json");
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Scene")) {
            PixelEngine::SceneSerializer serializer(*m_ActiveScene);
            if (serializer.Deserialize("assets/scenes/sandbox_scene.json")) {
                m_SelectedEntity = {}; // Reset selection as entities were re-created
            }
        }

        ImGui::End();

        // 4. Asset Database Window
        ImGui::Begin("Asset Database");
        auto& registry = PixelEngine::AssetManager::GetMetadataRegistry();
        ImGui::Text("Total Registered Assets: %zu", registry.size());
        ImGui::Separator();
        
        if (ImGui::BeginTable("AssetTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("UUID");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Path");
            ImGui::TableHeadersRow();

            for (const auto& [uuid, meta] : registry) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%llu", (uint64_t)uuid);
                ImGui::TableNextColumn();
                ImGui::Text("%s", meta.Type == PixelEngine::AssetType::Texture ? "Texture" :
                                  meta.Type == PixelEngine::AssetType::Shader ? "Shader" :
                                  meta.Type == PixelEngine::AssetType::Scene ? "Scene" :
                                  meta.Type == PixelEngine::AssetType::Audio ? "Audio" : "None");
                ImGui::TableNextColumn();
                ImGui::Text("%s", meta.SourcePath.c_str());
            }
            ImGui::EndTable();
        }
        ImGui::End();

        // Update Camera (Perspective for 3D)
        float aspect = m_OffscreenTarget->GetWidth() / (float)m_OffscreenTarget->GetHeight();
        m_Camera.SetPerspectiveProjection(glm::radians(45.0f), aspect, 0.1f, 100.0f);
        m_Camera.SetViewTarget(glm::vec3(3.0f, 3.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f));

        ImGui::ShowDemoWindow();
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
    std::unique_ptr<PixelEngine::GraphicsPipeline> m_UpscalePipeline;
    std::unique_ptr<PixelEngine::OffscreenTarget> m_OffscreenTarget;
    std::unique_ptr<PixelEngine::Buffer> m_QuadVertexBuffer;
    std::unique_ptr<PixelEngine::Buffer> m_QuadIndexBuffer;
    
    std::unique_ptr<PixelEngine::Scene> m_ActiveScene;
    std::unique_ptr<PixelEngine::RenderSystem> m_RenderSystem;
    PixelEngine::Entity m_SelectedEntity;

    PixelEngine::Camera m_Camera;
    float m_Rotation = 0.0f;

    int m_InternalWidth = 320;
    int m_InternalHeight = 180;
    bool m_PixelSnapping = true;
    glm::vec4 m_ObjectColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_RotationSpeed = 50.0f;
    float m_ObjectScale = 1.0f;

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
