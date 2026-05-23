#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include "Buffer.hpp"
#include "UBO.hpp"

struct SDL_Window;

namespace PixelEngine {

    class VulkanContext {
    public:
        VulkanContext() = default;
        ~VulkanContext();

        void Init(SDL_Window* window);
        void Shutdown();

        VkInstance GetInstance() const { return m_Instance; }
        VkDevice GetDevice() const { return m_Device; }
        VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
        VkSurfaceKHR GetSurface() const { return m_Surface; }
        uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
        VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
        VkRenderPass GetRenderPass() const { return m_RenderPass; }
        VkRenderPass GetOffscreenRenderPass() const { return m_OffscreenRenderPass; }
        VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
        VkExtent2D GetSwapchainExtent() const { return m_SwapchainExtent; }
        size_t GetSwapchainImageCount() const { return m_SwapchainImages.size(); }
        VkFramebuffer GetFramebuffer(uint32_t index) const { return m_SwapchainFramebuffers[index]; }
        VkCommandPool GetCommandPool() const { return m_CommandPool; }
        VkCommandBuffer GetCommandBuffer(uint32_t index) const { return m_CommandBuffers[index]; }
        
        VkDescriptorSetLayout GetGlobalDescriptorSetLayout() const { return m_GlobalDescriptorSetLayout; }
        VkDescriptorSetLayout GetTextureDescriptorSetLayout() const { return m_TextureDescriptorSetLayout; }
        VkDescriptorSetLayout GetUpscaleDescriptorSetLayout() const { return m_UpscaleDescriptorSetLayout; }
        
        VkDescriptorSet GetGlobalDescriptorSet(uint32_t index) const { return m_GlobalDescriptorSets[index]; }
        VkDescriptorSet GetUpscaleDescriptorSet(uint32_t index) const { return m_UpscaleDescriptorSets[index]; }

        VkDescriptorSet CreateTextureDescriptorSet(VkImageView imageView);

        VkPipelineLayout GetPipelineLayout() const { return m_PipelineLayout; }
        VkPipelineLayout GetUpscalePipelineLayout() const { return m_UpscalePipelineLayout; }
        VkSampler GetTextureSampler() const { return m_TextureSampler; }
        Buffer& GetUniformBuffer(uint32_t index) { return *m_UniformBuffers[index]; }
        VkDescriptorSet GetDefaultTextureDescriptorSet() const { return m_DefaultTextureDescriptorSet; }

        uint32_t AcquireNextImage(VkSemaphore signalSemaphore);
        void SubmitCommandBuffer(uint32_t imageIndex, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence);
        void PresentImage(uint32_t imageIndex, VkSemaphore waitSemaphore);

        void ResetQueryPool(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void WriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, uint32_t imageIndex, uint32_t queryIndex);
        void FetchQueryResults(uint32_t imageIndex);

        void UpdateUpscaleDescriptorSets(VkImageView colorImageView);

        float GetGPUTime() const { return m_GPUTime; }

        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
        VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

        VkCommandBuffer BeginSingleTimeCommands();
        void EndSingleTimeCommands(VkCommandBuffer commandBuffer);

    private:
        void CreateInstance();
        void SetupDebugMessenger();
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateSurface(SDL_Window* window);
        void CreateSwapchain(SDL_Window* window);
        void CreateImageViews();
        void CreateRenderPass();
        void CreateOffscreenRenderPass();
        void CreateDepthResources();
        void CreateFramebuffers();
        void CreateCommandPool();
        void CreateCommandBuffers();
        void CreateDescriptorPool();
        void CreateDescriptorSetLayout();
        void CreateUniformBuffers();
        void CreateDescriptorSets();
        void CreateTextureSampler();
        void CreateDefaultTexture();
        void CreateSyncObjects();
        void CreateQueryPool();

        void CleanupSwapchain();
        void RecreateSwapchain(SDL_Window* window);

        bool CheckValidationLayerSupport();
        std::vector<const char*> GetRequiredExtensions();

        VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window);
        VkFormat FindDepthFormat();

    private:
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        uint32_t m_GraphicsQueueFamily = 0;

        // Profiling
        VkQueryPool m_QueryPool = VK_NULL_HANDLE;
        float m_TimestampPeriod = 0.0f;
        float m_GPUTime = 0.0f;

        // Swapchain
        VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
        std::vector<VkImage> m_SwapchainImages;
        std::vector<VkImageView> m_SwapchainImageViews;
        VkFormat m_SwapchainImageFormat;
        VkExtent2D m_SwapchainExtent;

        // Depth Resources
        VkImage m_DepthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_DepthImageMemory = VK_NULL_HANDLE;
        VkImageView m_DepthImageView = VK_NULL_HANDLE;

        // Rendering Resources
        VkRenderPass m_RenderPass = VK_NULL_HANDLE;
        VkRenderPass m_OffscreenRenderPass = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> m_SwapchainFramebuffers;

        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_CommandBuffers;

        // Descriptors
        VkDescriptorPool m_DescriptorPool = VK_NULL_HANDLE;
        
        VkDescriptorSetLayout m_GlobalDescriptorSetLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout m_TextureDescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_GlobalDescriptorSets;

        VkDescriptorSetLayout m_UpscaleDescriptorSetLayout = VK_NULL_HANDLE;
        VkPipelineLayout m_UpscalePipelineLayout = VK_NULL_HANDLE;
        std::vector<VkDescriptorSet> m_UpscaleDescriptorSets;

        VkSampler m_TextureSampler = VK_NULL_HANDLE;

        // Default Texture
        VkImage m_DefaultImage = VK_NULL_HANDLE;
        VkDeviceMemory m_DefaultImageMemory = VK_NULL_HANDLE;
        VkImageView m_DefaultImageView = VK_NULL_HANDLE;
        VkDescriptorSet m_DefaultTextureDescriptorSet = VK_NULL_HANDLE;

        std::vector<std::unique_ptr<Buffer>> m_UniformBuffers;

        const std::vector<const char*> m_ValidationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };

#ifdef NDEBUG
        const bool m_EnableValidationLayers = false;
#else
        const bool m_EnableValidationLayers = true;
#endif
    };

}
