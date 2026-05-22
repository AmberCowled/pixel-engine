#pragma once

#include <vulkan/vulkan.h>
#include <vector>

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
        VkSwapchainKHR GetSwapchain() const { return m_Swapchain; }
        VkExtent2D GetSwapchainExtent() const { return m_SwapchainExtent; }
        size_t GetSwapchainImageCount() const { return m_SwapchainImages.size(); }
        VkFramebuffer GetFramebuffer(uint32_t index) const { return m_SwapchainFramebuffers[index]; }
        VkCommandPool GetCommandPool() const { return m_CommandPool; }
        VkCommandBuffer GetCommandBuffer(uint32_t index) const { return m_CommandBuffers[index]; }

        uint32_t AcquireNextImage(VkSemaphore signalSemaphore);
        void SubmitCommandBuffer(uint32_t imageIndex, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence);
        void PresentImage(uint32_t imageIndex, VkSemaphore waitSemaphore);

        uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
        VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    private:
        void CreateInstance();
        void SetupDebugMessenger();
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateSurface(SDL_Window* window);
        void CreateSwapchain(SDL_Window* window);
        void CreateImageViews();
        void CreateRenderPass();
        void CreateDepthResources();
        void CreateFramebuffers();
        void CreateCommandPool();
        void CreateCommandBuffers();
        void CreateSyncObjects();

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
        std::vector<VkFramebuffer> m_SwapchainFramebuffers;

        VkCommandPool m_CommandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> m_CommandBuffers;

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
