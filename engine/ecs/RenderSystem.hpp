#pragma once

#include <engine/renderer/VulkanContext.hpp>
#include <engine/renderer/GraphicsPipeline.hpp>
#include <engine/renderer/Buffer.hpp>
#include <engine/renderer/Camera.hpp>
#include "Scene.hpp"
#include <memory>

namespace PixelEngine {

    class RenderSystem {
    public:
        RenderSystem(VulkanContext& context);
        ~RenderSystem();

        void Render(VkCommandBuffer commandBuffer, uint32_t imageIndex, Scene& scene, const Camera& camera);

    private:
        VulkanContext& m_Context;

        std::unique_ptr<GraphicsPipeline> m_MeshPipeline;

        std::unique_ptr<Buffer> m_CubeVertexBuffer;
        std::unique_ptr<Buffer> m_CubeIndexBuffer;

        void CreatePipelines();
        void CreateDefaultMeshes();
    };

}
