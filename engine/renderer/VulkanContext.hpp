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

    private:
        void CreateInstance();
        void SetupDebugMessenger();
        void PickPhysicalDevice();
        void CreateLogicalDevice();
        void CreateSurface(SDL_Window* window);

        bool CheckValidationLayerSupport();
        std::vector<const char*> GetRequiredExtensions();

    private:
        VkInstance m_Instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT m_DebugMessenger = VK_NULL_HANDLE;
        VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
        VkDevice m_Device = VK_NULL_HANDLE;
        VkSurfaceKHR m_Surface = VK_NULL_HANDLE;

        VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
        uint32_t m_GraphicsQueueFamily = 0;

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
