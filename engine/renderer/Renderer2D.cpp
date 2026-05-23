#include "Renderer2D.hpp"
#include <engine/renderer/UBO.hpp>
#include <engine/base/Log.hpp>
#include <engine/assets/AssetManager.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <stdexcept>
#include <filesystem>

namespace PixelEngine {

    void Renderer2D::Init(VulkanContext& context) {
        s_Context = &context;
        ResetStats();

        // 1. Create dynamic vertex buffers (one per swapchain image)
        size_t swapchainImageCount = s_Context->GetSwapchainImageCount();
        s_VertexBuffers.resize(swapchainImageCount);
        VkDeviceSize vertexBufferSize = sizeof(Vertex) * MaxVertices;

        for (size_t i = 0; i < swapchainImageCount; i++) {
            s_VertexBuffers[i] = std::make_unique<Buffer>(
                *s_Context,
                vertexBufferSize,
                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
            );
            // Persistently map
            s_VertexBuffers[i]->Map();
        }

        // 2. Create static index buffer
        VkDeviceSize indexBufferSize = sizeof(uint16_t) * MaxIndices;
        s_IndexBuffer = std::make_unique<Buffer>(
            *s_Context,
            indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        s_IndexBuffer->Map();

        uint16_t* indices = (uint16_t*)s_IndexBuffer->GetMapped();
        for (uint32_t i = 0; i < MaxQuads; i++) {
            indices[i * 6 + 0] = i * 4 + 0;
            indices[i * 6 + 1] = i * 4 + 1;
            indices[i * 6 + 2] = i * 4 + 2;
            indices[i * 6 + 3] = i * 4 + 2;
            indices[i * 6 + 4] = i * 4 + 3;
            indices[i * 6 + 5] = i * 4 + 0;
        }
        s_IndexBuffer->Unmap();

        // 3. Create sprite pipelines
        CreatePipelines();

        PX_CORE_INFO("Renderer2D initialized successfully.");
    }

    void Renderer2D::Shutdown() {
        s_VertexBuffers.clear();
        s_IndexBuffer.reset();
        s_SpritePipelineOpaque.reset();
        s_SpritePipelineAlphaBlend.reset();
        s_SpritePipelineAdditive.reset();
        s_Context = nullptr;
        PX_CORE_INFO("Renderer2D shutdown.");
    }

    void Renderer2D::CreatePipelines() {
        std::vector<std::string> searchPaths = {
            "shaders/",
            "../shaders/",
            "../../shaders/",
            "build/bin/shaders/",
            "../bin/shaders/"
        };

        std::string spriteVert, spriteFrag;
        for (const auto& p : searchPaths) {
            if (spriteVert.empty() && std::filesystem::exists(p + "sprite.vert.spv")) spriteVert = p + "sprite.vert.spv";
            if (spriteFrag.empty() && std::filesystem::exists(p + "sprite.frag.spv")) spriteFrag = p + "sprite.frag.spv";
        }

        if (spriteVert.empty() || spriteFrag.empty()) {
            PX_CORE_CRITICAL("Renderer2D: Failed to find sprite shaders!");
            throw std::runtime_error("Renderer2D: Failed to find sprite shaders!");
        }

        // Base config info
        PipelineConfigInfo configInfo{};
        GraphicsPipeline::DefaultPipelineConfigInfo(configInfo);
        configInfo.renderPass = s_Context->GetOffscreenRenderPass();
        configInfo.pipelineLayout = s_Context->GetPipelineLayout();
        configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        // 1. Opaque Pipeline
        configInfo.colorBlendAttachment.blendEnable = VK_FALSE;
        s_SpritePipelineOpaque = std::make_unique<GraphicsPipeline>(*s_Context, spriteVert, spriteFrag, configInfo);

        // 2. AlphaBlend Pipeline
        configInfo.colorBlendAttachment.blendEnable = VK_TRUE;
        configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        s_SpritePipelineAlphaBlend = std::make_unique<GraphicsPipeline>(*s_Context, spriteVert, spriteFrag, configInfo);

        // 3. Additive Pipeline
        configInfo.colorBlendAttachment.blendEnable = VK_TRUE;
        configInfo.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        configInfo.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        configInfo.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        configInfo.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        configInfo.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        configInfo.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        s_SpritePipelineAdditive = std::make_unique<GraphicsPipeline>(*s_Context, spriteVert, spriteFrag, configInfo);
    }

    void Renderer2D::RecreatePipelines() {
        if (!s_Context) return;
        PX_CORE_INFO("Renderer2D: Recreating sprite pipelines...");
        s_SpritePipelineOpaque.reset();
        s_SpritePipelineAlphaBlend.reset();
        s_SpritePipelineAdditive.reset();
        CreatePipelines();
    }

    void Renderer2D::BeginScene(VkCommandBuffer cmd, uint32_t imageIndex, const Camera& camera) {
        s_ActiveCommandBuffer = cmd;
        s_CurrentImageIndex = imageIndex;
        s_ViewProj = camera.GetProjection() * camera.GetView();
        
        s_DrawQueue.clear();
        ResetStats();
    }

    void Renderer2D::EndScene() {
        if (s_DrawQueue.empty()) return;

        // Sort draw queue: RenderLayer -> BlendMode -> TextureID
        std::sort(s_DrawQueue.begin(), s_DrawQueue.end(), [](const DrawQuadCommand& a, const DrawQuadCommand& b) {
            if (a.RenderLayer != b.RenderLayer)
                return a.RenderLayer < b.RenderLayer;
            if (a.Blend != b.Blend)
                return static_cast<int>(a.Blend) < static_cast<int>(b.Blend);
            return a.TextureID < b.TextureID;
        });

        // Initialize batch
        StartBatch();

        for (const auto& quad : s_DrawQueue) {
            // Check if we need to start a new batch
            bool textureChanged = (quad.TextureID != (s_IndexCount > 0 ? s_DrawQueue[&quad - &s_DrawQueue[0] - 1].TextureID : quad.TextureID));
            bool blendChanged = (quad.Blend != (s_IndexCount > 0 ? s_DrawQueue[&quad - &s_DrawQueue[0] - 1].Blend : quad.Blend));
            bool bufferFull = (s_IndexCount + 6 >= MaxIndices);

            // Wait, if s_IndexCount is 0, we are starting the first quad in the batch, so no change can trigger next batch
            if (s_IndexCount > 0 && (textureChanged || blendChanged || bufferFull)) {
                NextBatch();
            }

            // If we just flushed, ensure base/ptr is valid
            if (s_VertexBufferPtr == nullptr) {
                s_VertexBufferPtr = s_VertexBufferBase;
            }

            // quad vertices layout:
            // 0: BL, 1: BR, 2: TR, 3: TL
            glm::vec4 qPos[4] = {
                { -0.5f, -0.5f, 0.0f, 1.0f },
                {  0.5f, -0.5f, 0.0f, 1.0f },
                {  0.5f,  0.5f, 0.0f, 1.0f },
                { -0.5f,  0.5f, 0.0f, 1.0f }
            };

            glm::vec2 uvs[4] = {
                { 0.0f, 0.0f },
                { 1.0f, 0.0f },
                { 1.0f, 1.0f },
                { 0.0f, 1.0f }
            };

            // Write vertices
            for (int i = 0; i < 4; i++) {
                s_VertexBufferPtr->pos = glm::vec3(quad.Transform * qPos[i]);
                s_VertexBufferPtr->color = quad.Color;
                s_VertexBufferPtr->uv = uvs[i];
                s_VertexBufferPtr++;
            }

            s_IndexCount += 6;
            s_Stats.QuadCount++;
        }

        // Flush the last batch
        if (s_IndexCount > 0) {
            Flush();
        }

        s_VertexBufferBase = nullptr;
        s_VertexBufferPtr = nullptr;
        s_IndexCount = 0;
    }

    void Renderer2D::StartBatch() {
        s_VertexBufferBase = (Vertex*)s_VertexBuffers[s_CurrentImageIndex]->GetMapped();
        s_VertexBufferPtr = s_VertexBufferBase;
        s_IndexCount = 0;
    }

    void Renderer2D::NextBatch() {
        Flush();
        StartBatch();
    }

    void Renderer2D::Flush() {
        if (s_IndexCount == 0) return;

        // 1. Get batch properties (from the last added command that caused/finished this batch)
        // Let's identify the index of the first command in the current batch.
        // The number of quads in this batch is:
        uint32_t quadCount = s_IndexCount / 6;
        uint32_t currentQuadIndexInQueue = s_Stats.QuadCount - quadCount;
        const auto& batchCommand = s_DrawQueue[currentQuadIndexInQueue];

        // 2. Bind Pipeline based on blend mode
        GraphicsPipeline* pipeline = nullptr;
        switch (batchCommand.Blend) {
            case BlendMode::Opaque:     pipeline = s_SpritePipelineOpaque.get(); break;
            case BlendMode::AlphaBlend: pipeline = s_SpritePipelineAlphaBlend.get(); break;
            case BlendMode::Additive:   pipeline = s_SpritePipelineAdditive.get(); break;
        }

        if (pipeline) {
            pipeline->Bind(s_ActiveCommandBuffer);
        }

        // 3. Bind descriptor sets (Global UBO, and Texture descriptor)
        VkDescriptorSet globalSet = s_Context->GetGlobalDescriptorSet(s_CurrentImageIndex);
        VkDescriptorSet textureSet = AssetManager::GetTextureDescriptorSet(batchCommand.TextureID);
        if (textureSet == VK_NULL_HANDLE) {
            textureSet = s_Context->GetDefaultTextureDescriptorSet();
        }

        std::array<VkDescriptorSet, 2> descriptorSets = { globalSet, textureSet };
        vkCmdBindDescriptorSets(
            s_ActiveCommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            s_Context->GetPipelineLayout(),
            0,
            static_cast<uint32_t>(descriptorSets.size()),
            descriptorSets.data(),
            0,
            nullptr
        );

        // 4. Set Push Constants (identity model, and white color, to bypass scaling/tinting in shader)
        PushConstantData push{};
        push.model = glm::mat4(1.0f);
        push.color = glm::vec4(1.0f);
        vkCmdPushConstants(
            s_ActiveCommandBuffer,
            s_Context->GetPipelineLayout(),
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(PushConstantData),
            &push
        );

        // 5. Bind Vertex & Index Buffers
        VkBuffer vertexBuffers[] = { s_VertexBuffers[s_CurrentImageIndex]->GetBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(s_ActiveCommandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(s_ActiveCommandBuffer, s_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

        // 6. Draw!
        vkCmdDrawIndexed(s_ActiveCommandBuffer, s_IndexCount, 1, 0, 0, 0);

        s_Stats.DrawCalls++;
    }

    void Renderer2D::SubmitQuad(const glm::mat4& transform, UUID textureID, const glm::vec4& color, BlendMode blend, int renderLayer) {
        s_DrawQueue.push_back({ transform, color, textureID, blend, renderLayer });
    }

    void Renderer2D::SubmitQuad(const glm::vec2& position, const glm::vec2& size, float rotation, UUID textureID, const glm::vec4& color, BlendMode blend, int renderLayer) {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(position, 0.0f));
        if (rotation != 0.0f) {
            transform = glm::rotate(transform, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        }
        transform = glm::scale(transform, glm::vec3(size, 1.0f));
        SubmitQuad(transform, textureID, color, blend, renderLayer);
    }

    void Renderer2D::ResetStats() {
        s_Stats.DrawCalls = 0;
        s_Stats.QuadCount = 0;
    }

    Renderer2D::Statistics Renderer2D::GetStats() {
        return s_Stats;
    }

}
