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
#include <imgui.h>
#include <glm/gtc/matrix_transform.hpp>
#include <chrono>
#include <iostream>
#include <array>

class SandboxApp : public PixelEngine::EngineApp {
public:
    SandboxApp() : PixelEngine::EngineApp({"Pixel Sandbox", 1280, 720}) {
        PX_INFO("Sandbox App Started.");
        
        auto& context = GetVulkanContext();

        // 1. Create Offscreen Target (320x180)
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

        // 2. Link Offscreen Target to Upscale pipeline
        context.UpdateUpscaleDescriptorSets(m_OffscreenTarget->GetColorImageView());

        // 3. Create Geometry Buffers
        // Sprite Quad
        VkDeviceSize spriteVertexBufferSize = sizeof(PixelEngine::SPRITE_VERTICES[0]) * PixelEngine::SPRITE_VERTICES.size();
        m_SpriteVertexBuffer = std::make_unique<PixelEngine::Buffer>(
            context, spriteVertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        m_SpriteVertexBuffer->Map();
        m_SpriteVertexBuffer->WriteToBuffer((void*)PixelEngine::SPRITE_VERTICES.data(), spriteVertexBufferSize);

        VkDeviceSize spriteIndexBufferSize = sizeof(PixelEngine::SPRITE_INDICES[0]) * PixelEngine::SPRITE_INDICES.size();
        m_SpriteIndexBuffer = std::make_unique<PixelEngine::Buffer>(
            context, spriteIndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        m_SpriteIndexBuffer->Map();
        m_SpriteIndexBuffer->WriteToBuffer((void*)PixelEngine::SPRITE_INDICES.data(), spriteIndexBufferSize);

        // Fullscreen Triangle
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

        // 4. Create Pipelines
        // Sprite Pipeline
        PixelEngine::PipelineConfigInfo spritePipelineConfig{};
        PixelEngine::GraphicsPipeline::DefaultPipelineConfigInfo(spritePipelineConfig);
        spritePipelineConfig.renderPass = context.GetOffscreenRenderPass();
        spritePipelineConfig.pipelineLayout = context.GetPipelineLayout();
        
        m_SpritePipeline = std::make_unique<PixelEngine::GraphicsPipeline>(
            context, "../shaders/sprite.vert.spv", "../shaders/sprite.frag.spv", spritePipelineConfig
        );

        // Upscale Pipeline
        PixelEngine::PipelineConfigInfo upscaleConfig{};
        PixelEngine::GraphicsPipeline::DefaultPipelineConfigInfo(upscaleConfig);
        upscaleConfig.renderPass = context.GetRenderPass();
        upscaleConfig.pipelineLayout = context.GetUpscalePipelineLayout();
        upscaleConfig.depthStencilInfo.depthTestEnable = VK_FALSE;
        upscaleConfig.depthStencilInfo.depthWriteEnable = VK_FALSE;
        upscaleConfig.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;

        m_UpscalePipeline = std::make_unique<PixelEngine::GraphicsPipeline>(
            context, "../shaders/upscale.vert.spv", "../shaders/upscale.frag.spv", upscaleConfig
        );

        // 5. Load Texture
        try {
            m_Texture = std::make_unique<PixelEngine::Texture>(context, "../assets/test.png");
            for (uint32_t i = 0; i < context.GetSwapchainImageCount(); i++) {
                context.UpdateDescriptorSets(i, m_Texture->GetImageView());
            }
        } catch (...) {
            PX_WARN("Could not load assets/test.png. Sprite will be untextured.");
        }
    }

    ~SandboxApp() {
        PX_INFO("Sandbox App Shutdown.");
    }

    void OnUpdate(float deltaTime) override {
        m_Rotation += deltaTime * m_RotationSpeed;

        auto& context = GetVulkanContext();
        
        // UI
        ImGui::Begin("Engine Controls");
        ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
        ImGui::Text("CPU Time: %.3f ms", deltaTime * 1000.0f);
        ImGui::Text("GPU Time: %.3f ms", context.GetGPUTime());
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
        ImGui::ColorEdit4("Sprite Color", &m_SpriteColor.x);
        ImGui::SliderFloat("Rotation Speed", &m_RotationSpeed, 0.0f, 200.0f);
        ImGui::SliderFloat("Sprite Scale", &m_SpriteScale, 1.0f, 512.0f);

        ImGui::End();

        // Update Camera (Orthographic)
        float aspect = m_OffscreenTarget->GetWidth() / (float)m_OffscreenTarget->GetHeight();
        float h = m_OffscreenTarget->GetHeight();
        float w = m_OffscreenTarget->GetWidth();
        m_Camera.SetOrthographicProjection(-w/2, w/2, -h/2, h/2, -1.0f, 1.0f);
        m_Camera.SetViewTarget(glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, 0.0f));

        // Update UBO
        PixelEngine::GlobalUBO ubo{};
        ubo.model = glm::mat4(1.0f);
        ubo.model = glm::rotate(ubo.model, glm::radians(m_Rotation), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.model = glm::scale(ubo.model, glm::vec3(m_SpriteScale, m_SpriteScale, 1.0f));

        ubo.view = m_Camera.GetView();
        ubo.proj = m_Camera.GetProjection();
        ubo.resolution = glm::vec2(m_OffscreenTarget->GetWidth(), m_OffscreenTarget->GetHeight());
        ubo.pixelSnapping = m_PixelSnapping ? 1.0f : 0.0f;
        ubo.baseColor = m_SpriteColor;

        uint32_t imageIndex = GetCurrentImageIndex();
        context.GetUniformBuffer(imageIndex).WriteToBuffer(&ubo, sizeof(ubo));

        ImGui::ShowDemoWindow();
    }

    void OnRender() override {
        uint32_t imageIndex = GetCurrentImageIndex();
        VkCommandBuffer commandBuffer = GetCurrentCommandBuffer();
        auto& context = GetVulkanContext();

        // Pass 1: Offscreen Sprite Render
        {
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = context.GetOffscreenRenderPass();
            renderPassInfo.framebuffer = m_OffscreenTarget->GetFramebuffer();
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = { m_OffscreenTarget->GetWidth(), m_OffscreenTarget->GetHeight() };

            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
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

            m_SpritePipeline->Bind(commandBuffer);

            VkBuffer vertexBuffers[] = {m_SpriteVertexBuffer->GetBuffer()};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
            vkCmdBindIndexBuffer(commandBuffer, m_SpriteIndexBuffer->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);

            VkDescriptorSet descriptorSet = context.GetDescriptorSet(imageIndex);
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context.GetPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);

            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(PixelEngine::SPRITE_INDICES.size()), 1, 0, 0, 0);

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

            // ImGui will be rendered here by EngineApp which will also end the pass
        }
    }

private:
    std::unique_ptr<PixelEngine::GraphicsPipeline> m_SpritePipeline;
    std::unique_ptr<PixelEngine::GraphicsPipeline> m_UpscalePipeline;
    std::unique_ptr<PixelEngine::OffscreenTarget> m_OffscreenTarget;
    std::unique_ptr<PixelEngine::Buffer> m_SpriteVertexBuffer;
    std::unique_ptr<PixelEngine::Buffer> m_SpriteIndexBuffer;
    std::unique_ptr<PixelEngine::Buffer> m_QuadVertexBuffer;
    std::unique_ptr<PixelEngine::Buffer> m_QuadIndexBuffer;
    std::unique_ptr<PixelEngine::Texture> m_Texture;
    
    PixelEngine::Camera m_Camera;
    float m_Rotation = 0.0f;

    int m_InternalWidth = 320;
    int m_InternalHeight = 180;
    bool m_PixelSnapping = true;
    glm::vec4 m_SpriteColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float m_RotationSpeed = 50.0f;
    float m_SpriteScale = 64.0f;
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
