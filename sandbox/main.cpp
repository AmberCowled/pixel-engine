#include <engine/core/EngineApp.hpp>
#include <engine/base/Log.hpp>
#include <engine/renderer/VulkanContext.hpp>
#include <engine/renderer/GraphicsPipeline.hpp>
#include <engine/renderer/Buffer.hpp>
#include <engine/renderer/Vertex.hpp>
#include <engine/renderer/UBO.hpp>
#include <engine/renderer/CubeData.hpp>
#include <engine/renderer/Camera.hpp>
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <iostream>

class SandboxApp : public PixelEngine::EngineApp {
public:
    SandboxApp() : PixelEngine::EngineApp({"Pixel Sandbox", 1280, 720}) {
        PX_INFO("Sandbox App Started.");
        
        auto& context = GetVulkanContext();

        // Create Vertex Buffer
        VkDeviceSize vertexBufferSize = sizeof(PixelEngine::CUBE_VERTICES[0]) * PixelEngine::CUBE_VERTICES.size();
        m_VertexBuffer = std::make_unique<PixelEngine::Buffer>(
            context,
            vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        m_VertexBuffer->Map();
        m_VertexBuffer->WriteToBuffer((void*)PixelEngine::CUBE_VERTICES.data(), vertexBufferSize);

        // Create Index Buffer
        VkDeviceSize indexBufferSize = sizeof(PixelEngine::CUBE_INDICES[0]) * PixelEngine::CUBE_INDICES.size();
        m_IndexBuffer = std::make_unique<PixelEngine::Buffer>(
            context,
            indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        m_IndexBuffer->Map();
        m_IndexBuffer->WriteToBuffer((void*)PixelEngine::CUBE_INDICES.data(), indexBufferSize);

        // Create Pipeline
        PixelEngine::PipelineConfigInfo pipelineConfig{};
        PixelEngine::GraphicsPipeline::DefaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass = context.GetRenderPass();
        pipelineConfig.pipelineLayout = context.GetPipelineLayout();
        
        m_Pipeline = std::make_unique<PixelEngine::GraphicsPipeline>(
            context,
            "../shaders/simple.vert.spv",
            "../shaders/simple.frag.spv",
            pipelineConfig
        );
    }

    ~SandboxApp() {
        PX_INFO("Sandbox App Shutdown.");
    }

    void OnUpdate(float deltaTime) override {
        m_Rotation += deltaTime * 50.0f;

        // Update Camera
        auto extent = GetVulkanContext().GetSwapchainExtent();
        m_Camera.SetPerspectiveProjection(glm::radians(45.0f), extent.width / (float)extent.height, 0.1f, 10.0f);
        m_Camera.SetViewTarget(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f));

        // Update UBO
        PixelEngine::GlobalUBO ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(m_Rotation), glm::vec3(0.5f, 1.0f, 0.0f));
        ubo.view = m_Camera.GetView();
        ubo.proj = m_Camera.GetProjection();

        GetVulkanContext().GetUniformBuffer(GetCurrentFrameIndex()).WriteToBuffer(&ubo, sizeof(ubo));

        // UI
        ImGui::Begin("Engine Status");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("DeltaTime: %.3f ms", deltaTime * 1000.0f);
        ImGui::End();

        ImGui::ShowDemoWindow();
    }

    void OnRender() override {
        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        
        // Set dynamic state
        auto extent = GetVulkanContext().GetSwapchainExtent();
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

        m_Pipeline->Bind(commandBuffer);

        VkBuffer vertexBuffers[] = {m_VertexBuffer->GetBuffer()};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_IndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

        VkDescriptorSet descriptorSet = GetVulkanContext().GetDescriptorSet(GetCurrentFrameIndex());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GetVulkanContext().GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(PixelEngine::CUBE_INDICES.size()), 1, 0, 0, 0);
    }

private:
    std::unique_ptr<PixelEngine::GraphicsPipeline> m_Pipeline;
    std::unique_ptr<PixelEngine::Buffer> m_VertexBuffer;
    std::unique_ptr<PixelEngine::Buffer> m_IndexBuffer;
    PixelEngine::Camera m_Camera;
    float m_Rotation = 0.0f;
};

int main(int argc, char* argv[]) {
    std::cerr << "Main started" << std::endl;
    try {
        SandboxApp* app = new SandboxApp();
        std::cerr << "App created" << std::endl;
        app->Run();
        std::cerr << "App finished" << std::endl;
        delete app;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        PX_CORE_CRITICAL("Unhandled Exception: {0}", e.what());
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception" << std::endl;
        PX_CORE_CRITICAL("Unknown Exception occurred!");
        return 1;
    }
    return 0;
}
