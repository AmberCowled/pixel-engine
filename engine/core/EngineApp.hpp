#pragma once

#include <string>
#include <memory>
#include <vector>
#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

namespace PixelEngine {

    class VulkanContext;

    struct AppConfig {
        std::string Name = "Pixel Engine App";
        uint32_t Width = 1280;
        uint32_t Height = 720;
    };

    class EngineApp {
    public:
        EngineApp(const AppConfig& config = AppConfig());
        virtual ~EngineApp();

        void Run();
        void Close();

        VulkanContext& GetVulkanContext() { return *m_VulkanContext; }
        uint32_t GetCurrentFrameIndex() const { return m_CurrentFrame; }
        uint32_t GetCurrentImageIndex() const { return m_ImageIndex; }
        VkCommandBuffer GetCurrentCommandBuffer() const;

        virtual void OnUpdate(float deltaTime) {}
        virtual void OnRender() {}
        virtual void OnEvent(SDL_Event& event) {}

        // Manual render pass control for multi-pass setups
        void BeginSwapChainRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex, glm::vec4 clearColor = {0.1f, 0.1f, 0.1f, 1.0f});
        void EndSwapChainRenderPass(VkCommandBuffer commandBuffer);

    protected:
        bool m_Running = true;
        SDL_Window* m_Window = nullptr;
        AppConfig m_Config;
        std::unique_ptr<VulkanContext> m_VulkanContext;
        bool m_FramebufferResized = false;

    private:
        void ProcessEvents();
        void InitImGui();
        void ShutdownImGui();
        
    public:
        void BeginFrame();
        void EndFrame();

    private:
        VkDescriptorPool m_ImGuiDescriptorPool = VK_NULL_HANDLE;

        // Frame Sync
        uint32_t m_CurrentFrame = 0;
        uint32_t m_ImageIndex = 0;
        uint32_t m_CurrentAcquireSemIndex = 0;
        static const int MAX_FRAMES_IN_FLIGHT = 2;
        static const int MAX_SWAPCHAIN_IMAGES = 3;
        VkSemaphore m_ImageAvailableSemaphores[MAX_SWAPCHAIN_IMAGES];
        VkSemaphore m_RenderFinishedSemaphores[MAX_SWAPCHAIN_IMAGES];
        VkFence m_InFlightFences[MAX_FRAMES_IN_FLIGHT];

        uint32_t m_ImageIndexHistory[MAX_FRAMES_IN_FLIGHT]{};
        bool m_FrameHasFinished[MAX_FRAMES_IN_FLIGHT]{};
    };

    // To be defined in client
    EngineApp* CreateApplication();

}
