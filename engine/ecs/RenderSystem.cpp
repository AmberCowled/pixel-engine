#include "RenderSystem.hpp"
#include "Components.hpp"
#include "Entity.hpp"
#include <engine/renderer/CubeData.hpp>
#include <engine/renderer/Renderer2D.hpp>
#include <engine/assets/AssetManager.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <array>
#include <filesystem>
#include <engine/base/Log.hpp>

namespace PixelEngine {

    RenderSystem::RenderSystem(VulkanContext& context) : m_Context(context) {
        CreatePipelines();
        CreateDefaultMeshes();
    }

    RenderSystem::~RenderSystem() {}

    void RenderSystem::CreatePipelines() {
        std::vector<std::string> searchPaths = {
            "shaders/",
            "../shaders/",
            "../../shaders/",
            "build/bin/shaders/",
            "../bin/shaders/"
        };

        std::string simpleVert, simpleFrag;
        for (const auto& p : searchPaths) {
            if (simpleVert.empty() && std::filesystem::exists(p + "simple.vert.spv")) simpleVert = p + "simple.vert.spv";
            if (simpleFrag.empty() && std::filesystem::exists(p + "simple.frag.spv")) simpleFrag = p + "simple.frag.spv";
        }

        if (simpleVert.empty() || simpleFrag.empty()) PX_CORE_CRITICAL("Failed to find 3D shaders!");

        // Mesh Pipeline (3D)
        PipelineConfigInfo meshConfig{};
        GraphicsPipeline::DefaultPipelineConfigInfo(meshConfig);
        meshConfig.renderPass = m_Context.GetOffscreenRenderPass();
        meshConfig.pipelineLayout = m_Context.GetPipelineLayout();
        
        m_MeshPipeline = std::make_unique<GraphicsPipeline>(
            m_Context, simpleVert, simpleFrag, meshConfig
        );
    }

    void RenderSystem::CreateDefaultMeshes() {
        // Cube
        VkDeviceSize cubeVertexSize = sizeof(CUBE_VERTICES[0]) * CUBE_VERTICES.size();
        m_CubeVertexBuffer = std::make_unique<Buffer>(m_Context, cubeVertexSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_CubeVertexBuffer->Map();
        m_CubeVertexBuffer->WriteToBuffer((void*)CUBE_VERTICES.data(), cubeVertexSize);

        VkDeviceSize cubeIndexSize = sizeof(CUBE_INDICES[0]) * CUBE_INDICES.size();
        m_CubeIndexBuffer = std::make_unique<Buffer>(m_Context, cubeIndexSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        m_CubeIndexBuffer->Map();
        m_CubeIndexBuffer->WriteToBuffer((void*)CUBE_INDICES.data(), cubeIndexSize);
    }

    void RenderSystem::Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, Scene& scene, const Camera& camera) {
        // Update Global UBO
        GlobalUBO ubo{};
        ubo.view = camera.GetView();
        ubo.proj = camera.GetProjection();
        ubo.resolution = glm::vec2(320, 180); // TODO: Dynamic
        ubo.pixelSnapping = 1.0f;

        m_Context.GetUniformBuffer(imageIndex).WriteToBuffer(&ubo, sizeof(ubo));

        VkDescriptorSet globalSet = m_Context.GetGlobalDescriptorSet(imageIndex);

        // Render Meshes (Cubes)
        m_MeshPipeline->Bind(commandBuffer);
        
        auto meshView = scene.m_Registry.view<TransformComponent, MeshRendererComponent>();
        for (auto entity : meshView) {
            auto& transform = meshView.get<TransformComponent>(entity);
            auto& mesh = meshView.get<MeshRendererComponent>(entity);
            if (!mesh.Enabled) continue;

            // Push Constants
            PushConstantData push{};
            push.model = scene.GetWorldTransform({entity, &scene});
            push.color = mesh.Color;
            vkCmdPushConstants(commandBuffer, m_Context.GetPipelineLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &push);

            // Bind Descriptor Sets
            VkDescriptorSet textureSet = AssetManager::GetTextureDescriptorSet(mesh.TextureID);
            if (textureSet == VK_NULL_HANDLE) {
                textureSet = m_Context.GetDefaultTextureDescriptorSet();
            }

            std::array<VkDescriptorSet, 2> sets = { globalSet, textureSet };
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_Context.GetPipelineLayout(), 0, static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

            VkBuffer vertexBuffers[] = {m_CubeVertexBuffer->GetBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, m_CubeIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(CUBE_INDICES.size()), 1, 0, 0, 0);
        }

        // Render Sprites via Renderer2D
        Renderer2D::BeginScene(commandBuffer, imageIndex, camera);
        
        auto spriteView = scene.m_Registry.view<TransformComponent, SpriteRendererComponent>();
        for (auto entity : spriteView) {
            auto& transform = spriteView.get<TransformComponent>(entity);
            auto& sprite = spriteView.get<SpriteRendererComponent>(entity);
            if (!sprite.Enabled) continue;

            Renderer2D::SubmitQuad(
                scene.GetWorldTransform({entity, &scene}),
                sprite.Mat.TextureID,
                sprite.Mat.Color,
                sprite.Mat.Blend,
                0, // Render Layer
                sprite.Mat.UVs
            );
        }

        // Render Tilemaps via Renderer2D
        auto tilemapView = scene.m_Registry.view<TransformComponent, TilemapComponent>();
        for (auto entity : tilemapView) {
            auto& tilemap = tilemapView.get<TilemapComponent>(entity);
            if (!tilemap.Enabled) continue;
            auto tileset = AssetManager::GetTileset(tilemap.TilesetID);
            if (!tileset || tileset->TextureID == 0) continue;

            auto tex = AssetManager::GetTexture(tileset->TextureID);
            if (!tex) continue;

            float texWidth = (float)tex->GetWidth();
            float texHeight = (float)tex->GetHeight();
            uint32_t tileSize = tileset->TileSize;
            uint32_t cols = static_cast<uint32_t>(texWidth) / tileSize;
            if (cols == 0) cols = 1;

            glm::mat4 worldTransform = scene.GetWorldTransform({entity, &scene});

            for (const auto& [coords, chunk] : tilemap.Chunks) {
                int cx = coords.first;
                int cy = coords.second;

                for (int ty = 0; ty < TilemapChunk::ChunkSize; ty++) {
                    for (int tx = 0; tx < TilemapChunk::ChunkSize; tx++) {
                        int idx = ty * TilemapChunk::ChunkSize + tx;
                        uint32_t tileIndex = chunk.Tiles[idx].TileIndex;
                        if (tileIndex == 0) continue; // Empty

                        // Calculate tile local position (Y-up coordinates)
                        float xLocal = static_cast<float>(cx * TilemapChunk::ChunkSize + tx);
                        float yLocal = static_cast<float>(cy * TilemapChunk::ChunkSize + ty);

                        glm::mat4 tileLocalTransform = glm::translate(glm::mat4(1.0f), glm::vec3(xLocal, yLocal, 0.0f));
                        glm::mat4 tileWorldTransform = worldTransform * tileLocalTransform;

                        // Calculate UVs (Y-up layout in tileset image)
                        uint32_t col = (tileIndex - 1) % cols;
                        uint32_t row = (tileIndex - 1) / cols;

                        float uMin = (col * tileSize) / texWidth;
                        float uMax = ((col + 1) * tileSize) / texWidth;
                        float vMin = (row * tileSize) / texHeight;
                        float vMax = ((row + 1) * tileSize) / texHeight;

                        std::array<glm::vec2, 4> tileUVs = {
                            glm::vec2{ uMin, vMin },
                            glm::vec2{ uMax, vMin },
                            glm::vec2{ uMax, vMax },
                            glm::vec2{ uMin, vMax }
                        };

                        Renderer2D::SubmitQuad(
                            tileWorldTransform,
                            tileset->TextureID,
                            glm::vec4(1.0f),
                            BlendMode::AlphaBlend,
                            tilemap.RenderLayer,
                            tileUVs
                        );
                    }
                }
            }
        }

        Renderer2D::EndScene();
    }

}
