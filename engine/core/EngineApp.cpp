#include "EngineApp.hpp"
#include <engine/base/Log.hpp>
#include <engine/renderer/VulkanContext.hpp>
#include <imgui.h>
#include <array>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>

namespace PixelEngine {

    EngineApp::EngineApp(const AppConfig& config) 
        : m_Config(config) {
        
        Log::Init();
        PX_CORE_INFO("Initializing Engine...");

        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == false) {
            PX_CORE_CRITICAL("SDL could not initialize! SDL_Error: {0}", SDL_GetError());
            m_Running = false;
            return;
        }

        m_Window = SDL_CreateWindow(
            m_Config.Name.c_str(),
            m_Config.Width,
            m_Config.Height,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
        );

        if (!m_Window) {
            PX_CORE_CRITICAL("Window could not be created! SDL_Error: {0}", SDL_GetError());
            m_Running = false;
            return;
        }

        m_VulkanContext = std::make_unique<VulkanContext>();
        m_VulkanContext->Init(m_Window);

        // Sync objects
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_SWAPCHAIN_IMAGES; i++) {
            if (vkCreateSemaphore(m_VulkanContext->GetDevice(), &semaphoreInfo, nullptr, &m_ImageAvailableSemaphores[i]) != VK_SUCCESS) {
                PX_CORE_CRITICAL("Failed to create acquisition semaphore {0}", i);
            }
            if (vkCreateSemaphore(m_VulkanContext->GetDevice(), &semaphoreInfo, nullptr, &m_RenderFinishedSemaphores[i]) != VK_SUCCESS) {
                PX_CORE_CRITICAL("Failed to create render finished semaphore {0}", i);
            }
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateFence(m_VulkanContext->GetDevice(), &fenceInfo, nullptr, &m_InFlightFences[i]) != VK_SUCCESS) {
                PX_CORE_CRITICAL("Failed to create in-flight fence {0}", i);
            }
        }

        InitImGui();

        PX_CORE_INFO("Engine Initialized Successfully.");
    }

    EngineApp::~EngineApp() {
        ShutdownImGui();

        for (size_t i = 0; i < MAX_SWAPCHAIN_IMAGES; i++) {
            vkDestroySemaphore(m_VulkanContext->GetDevice(), m_ImageAvailableSemaphores[i], nullptr);
            vkDestroySemaphore(m_VulkanContext->GetDevice(), m_RenderFinishedSemaphores[i], nullptr);
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroyFence(m_VulkanContext->GetDevice(), m_InFlightFences[i], nullptr);
        }

        if (m_Window) {
            SDL_DestroyWindow(m_Window);
        }
        SDL_Quit();
        PX_CORE_INFO("Engine Shutdown.");
    }

    void EngineApp::Run() {
        uint64_t lastTime = SDL_GetTicks();

        while (m_Running) {
            uint64_t currentTime = SDL_GetTicks();
            float deltaTime = (currentTime - lastTime) / 1000.0f;
            lastTime = currentTime;

            ProcessEvents();

            if (m_Running) {
                BeginFrame();
                
                OnUpdate(deltaTime);
                OnRender();

                EndFrame();
            }
        }

        vkDeviceWaitIdle(m_VulkanContext->GetDevice());
    }

    void EngineApp::Close() {
        m_Running = false;
    }

    VkCommandBuffer EngineApp::GetCurrentCommandBuffer() const {
        return m_VulkanContext->GetCommandBuffer(m_ImageIndex);
    }

    void EngineApp::ProcessEvents() {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                m_Running = false;
            }

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_Window)) {
                m_Running = false;
            }

            OnEvent(event);
        }
    }

    void EngineApp::InitImGui() {
        // Descriptor Pool
        VkDescriptorPoolSize pool_sizes[] = {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };
        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000 * (uint32_t)std::size(pool_sizes);
        pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;
        vkCreateDescriptorPool(m_VulkanContext->GetDevice(), &pool_info, nullptr, &m_ImGuiDescriptorPool);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        ImGui_ImplSDL3_InitForVulkan(m_Window);
        
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = m_VulkanContext->GetInstance();
        init_info.PhysicalDevice = m_VulkanContext->GetPhysicalDevice();
        init_info.Device = m_VulkanContext->GetDevice();
        init_info.QueueFamily = m_VulkanContext->GetGraphicsQueueFamily();
        init_info.Queue = m_VulkanContext->GetGraphicsQueue();
        init_info.DescriptorPool = m_ImGuiDescriptorPool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = (uint32_t)m_VulkanContext->GetSwapchainImageCount();
        
        init_info.PipelineInfoMain.RenderPass = m_VulkanContext->GetRenderPass();
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&init_info);
    }

    void EngineApp::ShutdownImGui() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        vkDestroyDescriptorPool(m_VulkanContext->GetDevice(), m_ImGuiDescriptorPool, nullptr);
    }

    void EngineApp::BeginFrame() {
        vkWaitForFences(m_VulkanContext->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame], VK_TRUE, UINT64_MAX);
        
        if (m_FrameHasFinished[m_CurrentFrame]) {
            m_VulkanContext->FetchQueryResults(m_ImageIndexHistory[m_CurrentFrame]);
        }

        vkResetFences(m_VulkanContext->GetDevice(), 1, &m_InFlightFences[m_CurrentFrame]);

        // Use a rotating set of semaphores for acquisition to avoid re-signaling while in use by swapchain
        static uint32_t acquireSemIndex = 0;
        m_ImageIndex = m_VulkanContext->AcquireNextImage(m_ImageAvailableSemaphores[acquireSemIndex]);
        
        m_CurrentAcquireSemIndex = acquireSemIndex;
        acquireSemIndex = (acquireSemIndex + 1) % MAX_SWAPCHAIN_IMAGES;

        VkCommandBuffer commandBuffer = m_VulkanContext->GetCommandBuffer(m_ImageIndex);
        vkResetCommandBuffer(commandBuffer, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        m_VulkanContext->ResetQueryPool(commandBuffer, m_ImageIndex);
        m_VulkanContext->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_ImageIndex, 0);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
    }

    void EngineApp::BeginSwapChainRenderPass(VkCommandBuffer commandBuffer, uint32_t imageIndex, glm::vec4 clearColor) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = m_VulkanContext->GetRenderPass();
        renderPassInfo.framebuffer = m_VulkanContext->GetFramebuffer(imageIndex);
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = m_VulkanContext->GetSwapchainExtent();

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{clearColor.r, clearColor.g, clearColor.b, clearColor.a}};
        clearValues[1].depthStencil = {1.0f, 0};

        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void EngineApp::EndSwapChainRenderPass(VkCommandBuffer commandBuffer) {
        vkCmdEndRenderPass(commandBuffer);
    }

    void EngineApp::EndFrame() {
        ImGui::Render();
        
        VkCommandBuffer commandBuffer = m_VulkanContext->GetCommandBuffer(m_ImageIndex);
        
        // ImGui rendering must happen within the active render pass
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
        
        // End the active render pass
        vkCmdEndRenderPass(commandBuffer);
        
        m_VulkanContext->WriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_ImageIndex, 1);

        // Now it's safe to end the command buffer
        vkEndCommandBuffer(commandBuffer);

        m_ImageIndexHistory[m_CurrentFrame] = m_ImageIndex;
        m_FrameHasFinished[m_CurrentFrame] = true;

        m_VulkanContext->SubmitCommandBuffer(m_ImageIndex, m_ImageAvailableSemaphores[m_CurrentAcquireSemIndex], m_RenderFinishedSemaphores[m_ImageIndex], m_InFlightFences[m_CurrentFrame]);
        m_VulkanContext->PresentImage(m_ImageIndex, m_RenderFinishedSemaphores[m_ImageIndex]);

        m_CurrentFrame = (m_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

}
