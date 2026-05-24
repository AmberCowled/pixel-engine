#pragma once

#include <engine/renderer/VulkanContext.hpp>
#include <engine/renderer/Buffer.hpp>
#include <engine/renderer/Camera.hpp>
#include <engine/renderer/Vertex.hpp>
#include <engine/renderer/Material.hpp>
#include <engine/renderer/GraphicsPipeline.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>

namespace PixelEngine {

    struct DrawQuadCommand {
        glm::mat4 Transform;
        glm::vec4 Color;
        UUID TextureID;
        BlendMode Blend;
        int RenderLayer;
        std::array<glm::vec2, 4> UVs;
    };

    class Renderer2D {
    public:
        struct Statistics {
            uint32_t DrawCalls = 0;
            uint32_t QuadCount = 0;
        };

        static void Init(VulkanContext& context);
        static void Shutdown();

        static void BeginScene(VkCommandBuffer cmd, uint32_t imageIndex, const Camera& camera);
        static void EndScene();
        static void Flush();

        static void SubmitQuad(const glm::mat4& transform, UUID textureID, const glm::vec4& color, BlendMode blend = BlendMode::AlphaBlend, int renderLayer = 0);
        static void SubmitQuad(const glm::mat4& transform, UUID textureID, const glm::vec4& color, BlendMode blend, int renderLayer, const std::array<glm::vec2, 4>& uvs);
        static void SubmitQuad(const glm::vec2& position, const glm::vec2& size, float rotation, UUID textureID, const glm::vec4& color, BlendMode blend = BlendMode::AlphaBlend, int renderLayer = 0);
        static void SubmitQuad(const glm::vec2& position, const glm::vec2& size, float rotation, UUID textureID, const glm::vec4& color, BlendMode blend, int renderLayer, const std::array<glm::vec2, 4>& uvs);

        static void ResetStats();
        static Statistics GetStats();

        // Hot reloading access
        static void RecreatePipelines();

    private:
        static void StartBatch();
        static void NextBatch();
        static void CreatePipelines();

    private:
        static inline VulkanContext* s_Context = nullptr;
        
        static constexpr uint32_t MaxQuads = 10000;
        static constexpr uint32_t MaxVertices = MaxQuads * 4;
        static constexpr uint32_t MaxIndices = MaxQuads * 6;

        // One dynamic vertex buffer per swapchain image
        static inline std::vector<std::unique_ptr<Buffer>> s_VertexBuffers;
        static inline std::unique_ptr<Buffer> s_IndexBuffer;

        static inline std::vector<DrawQuadCommand> s_DrawQueue;

        // Current frame info
        static inline VkCommandBuffer s_ActiveCommandBuffer = VK_NULL_HANDLE;
        static inline uint32_t s_CurrentImageIndex = 0;
        static inline glm::mat4 s_ViewProj = glm::mat4(1.0f);

        // Batch pointers
        static inline Vertex* s_VertexBufferBase = nullptr;
        static inline Vertex* s_VertexBufferPtr = nullptr;
        static inline uint32_t s_IndexCount = 0;

        // Pipelines for each blend mode
        static inline std::unique_ptr<GraphicsPipeline> s_SpritePipelineOpaque;
        static inline std::unique_ptr<GraphicsPipeline> s_SpritePipelineAlphaBlend;
        static inline std::unique_ptr<GraphicsPipeline> s_SpritePipelineAdditive;

        static inline Statistics s_Stats;
    };

}
