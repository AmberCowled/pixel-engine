#include "VulkanContext.hpp"
#include <engine/base/Log.hpp>
#include <engine/renderer/UBO.hpp>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL.h>
#include <set>
#include <string>
#include <algorithm>
#include <limits>
#include <array>

namespace PixelEngine {

    VulkanContext::~VulkanContext() {
        Shutdown();
    }

    void VulkanContext::Init(SDL_Window* window) {
        CreateInstance();
        SetupDebugMessenger();
        CreateSurface(window);
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapchain(window);
        CreateImageViews();
        CreateRenderPass();
        CreateOffscreenRenderPass();
        CreateDepthResources();
        CreateFramebuffers();
        CreateCommandPool();
        CreateCommandBuffers();
        CreateDescriptorPool();
        CreateDescriptorSetLayout();
        CreateUniformBuffers();
        CreateTextureSampler();
        CreateDefaultTexture();
        CreateDescriptorSets();
        CreateQueryPool();

        PX_CORE_INFO("Vulkan Context Initialized.");
    }

    void VulkanContext::Shutdown() {
        if (m_Device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_Device);
            
            CleanupSwapchain();

            if (m_DefaultImageView != VK_NULL_HANDLE) {
                vkDestroyImageView(m_Device, m_DefaultImageView, nullptr);
            }
            if (m_DefaultImage != VK_NULL_HANDLE) {
                vkDestroyImage(m_Device, m_DefaultImage, nullptr);
                vkFreeMemory(m_Device, m_DefaultImageMemory, nullptr);
            }

            if (m_QueryPool != VK_NULL_HANDLE) {
                vkDestroyQueryPool(m_Device, m_QueryPool, nullptr);
                m_QueryPool = VK_NULL_HANDLE;
            }

            if (m_DescriptorPool != VK_NULL_HANDLE) {
                vkDestroyDescriptorPool(m_Device, m_DescriptorPool, nullptr);
                m_DescriptorPool = VK_NULL_HANDLE;
            }

            if (m_TextureSampler != VK_NULL_HANDLE) {
                vkDestroySampler(m_Device, m_TextureSampler, nullptr);
                m_TextureSampler = VK_NULL_HANDLE;
            }

            if (m_GlobalDescriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_Device, m_GlobalDescriptorSetLayout, nullptr);
                m_GlobalDescriptorSetLayout = VK_NULL_HANDLE;
            }
            if (m_TextureDescriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_Device, m_TextureDescriptorSetLayout, nullptr);
                m_TextureDescriptorSetLayout = VK_NULL_HANDLE;
            }
            if (m_UpscaleDescriptorSetLayout != VK_NULL_HANDLE) {
                vkDestroyDescriptorSetLayout(m_Device, m_UpscaleDescriptorSetLayout, nullptr);
                m_UpscaleDescriptorSetLayout = VK_NULL_HANDLE;
            }

            if (m_PipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_Device, m_PipelineLayout, nullptr);
                m_PipelineLayout = VK_NULL_HANDLE;
            }
            if (m_UpscalePipelineLayout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(m_Device, m_UpscalePipelineLayout, nullptr);
                m_UpscalePipelineLayout = VK_NULL_HANDLE;
            }

            if (m_CommandPool != VK_NULL_HANDLE) {
                vkDestroyCommandPool(m_Device, m_CommandPool, nullptr);
                m_CommandPool = VK_NULL_HANDLE;
            }

            if (m_OffscreenRenderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_Device, m_OffscreenRenderPass, nullptr);
                m_OffscreenRenderPass = VK_NULL_HANDLE;
            }

            if (m_RenderPass != VK_NULL_HANDLE) {
                vkDestroyRenderPass(m_Device, m_RenderPass, nullptr);
                m_RenderPass = VK_NULL_HANDLE;
            }

            m_UniformBuffers.clear();

            vkDestroyDevice(m_Device, nullptr);
            m_Device = VK_NULL_HANDLE;
        }

        if (m_Surface != VK_NULL_HANDLE) {
            vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
            m_Surface = VK_NULL_HANDLE;
        }

        if (m_DebugMessenger != VK_NULL_HANDLE) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
                func(m_Instance, m_DebugMessenger, nullptr);
            }
            m_DebugMessenger = VK_NULL_HANDLE;
        }

        if (m_Instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_Instance, nullptr);
            m_Instance = VK_NULL_HANDLE;
        }
    }

    void VulkanContext::CreateInstance() {
        if (m_EnableValidationLayers && !CheckValidationLayerSupport()) {
            PX_CORE_CRITICAL("Validation layers requested, but not available!");
        }

        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Pixel Engine";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "Pixel Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_3;

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        auto extensions = GetRequiredExtensions();
        createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        if (m_EnableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(m_ValidationLayers.size());
            createInfo.ppEnabledLayerNames = m_ValidationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        if (vkCreateInstance(&createInfo, nullptr, &m_Instance) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create Vulkan instance!");
        }
    }

    void VulkanContext::SetupDebugMessenger() {
        if (!m_EnableValidationLayers) return;

        VkDebugUtilsMessengerCreateInfoEXT createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        createInfo.pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) -> VkBool32 {
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                PX_CORE_ERROR("Vulkan Validation Error: {0}", pCallbackData->pMessage);
            } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                PX_CORE_WARN("Vulkan Validation Warning: {0}", pCallbackData->pMessage);
            }
            return VK_FALSE;
        };

        auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_Instance, "vkCreateDebugUtilsMessengerEXT");
        if (func != nullptr) {
            func(m_Instance, &createInfo, nullptr, &m_DebugMessenger);
        }
    }

    void VulkanContext::CreateSurface(SDL_Window* window) {
        if (!SDL_Vulkan_CreateSurface(window, m_Instance, nullptr, &m_Surface)) {
            PX_CORE_CRITICAL("Failed to create window surface!");
        }
    }

    void VulkanContext::PickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
        if (deviceCount == 0) PX_CORE_CRITICAL("Failed to find GPUs with Vulkan support!");
        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

        for (const auto& device : devices) {
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
                m_PhysicalDevice = device;
                PX_CORE_INFO("Selected GPU: {0}", deviceProperties.deviceName);
                break;
            }
        }

        if (m_PhysicalDevice == VK_NULL_HANDLE) {
            m_PhysicalDevice = devices[0];
            VkPhysicalDeviceProperties deviceProperties;
            vkGetPhysicalDeviceProperties(m_PhysicalDevice, &deviceProperties);
            PX_CORE_INFO("Selected GPU: {0}", deviceProperties.deviceName);
        }
    }

    void VulkanContext::CreateLogicalDevice() {
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                m_GraphicsQueueFamily = i;
                break;
            }
            i++;
        }

        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_FALSE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.pEnabledFeatures = &deviceFeatures;

        std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(m_PhysicalDevice, &createInfo, nullptr, &m_Device) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create logical device!");
        }

        vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    }

    void VulkanContext::CreateSwapchain(SDL_Window* window) {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, nullptr);
        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, presentModes.data());

        VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(formats);
        VkPresentModeKHR presentMode = ChooseSwapPresentMode(presentModes);
        VkExtent2D extent = ChooseSwapExtent(capabilities, window);

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = m_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(m_Device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create swapchain!");
        }

        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
        m_SwapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_SwapchainImages.data());

        m_SwapchainImageFormat = surfaceFormat.format;
        m_SwapchainExtent = extent;
    }

    void VulkanContext::CreateImageViews() {
        m_SwapchainImageViews.resize(m_SwapchainImages.size());

        for (size_t i = 0; i < m_SwapchainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = m_SwapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = m_SwapchainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(m_Device, &createInfo, nullptr, &m_SwapchainImageViews[i]) != VK_SUCCESS) {
                PX_CORE_CRITICAL("Failed to create image views!");
            }
        }
    }

    void VulkanContext::CreateOffscreenRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = VK_FORMAT_R8G8B8A8_SRGB;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = FindDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependencies[2];
        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 2;
        renderPassInfo.pDependencies = dependencies;

        if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_OffscreenRenderPass) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create offscreen render pass!");
        }
    }

    void VulkanContext::CreateRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = m_SwapchainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = FindDepthFormat();
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(m_Device, &renderPassInfo, nullptr, &m_RenderPass) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create render pass!");
        }
    }

    void VulkanContext::CreateDepthResources() {
        VkFormat depthFormat = FindDepthFormat();
        CreateImage(m_SwapchainExtent.width, m_SwapchainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DepthImage, m_DepthImageMemory);
        m_DepthImageView = CreateImageView(m_DepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    void VulkanContext::CreateFramebuffers() {
        m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());

        for (size_t i = 0; i < m_SwapchainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments = { m_SwapchainImageViews[i], m_DepthImageView };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = m_RenderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = m_SwapchainExtent.width;
            framebufferInfo.height = m_SwapchainExtent.height;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(m_Device, &framebufferInfo, nullptr, &m_SwapchainFramebuffers[i]) != VK_SUCCESS) {
                PX_CORE_CRITICAL("Failed to create framebuffer!");
            }
        }
    }

    void VulkanContext::CreateQueryPool() {
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(m_PhysicalDevice, &properties);
        m_TimestampPeriod = properties.limits.timestampPeriod;

        if (!properties.limits.timestampComputeAndGraphics) {
            PX_CORE_WARN("Device does not support timestamp queries for graphics queue.");
            return;
        }

        VkQueryPoolCreateInfo queryPoolInfo{};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = static_cast<uint32_t>(m_SwapchainImages.size()) * 2;

        if (vkCreateQueryPool(m_Device, &queryPoolInfo, nullptr, &m_QueryPool) != VK_SUCCESS) {
            PX_CORE_ERROR("Failed to create query pool!");
        }
    }

    void VulkanContext::ResetQueryPool(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        if (m_QueryPool == VK_NULL_HANDLE) return;
        vkCmdResetQueryPool(commandBuffer, m_QueryPool, imageIndex * 2, 2);
    }

    void VulkanContext::WriteTimestamp(VkCommandBuffer commandBuffer, VkPipelineStageFlagBits pipelineStage, uint32_t imageIndex, uint32_t queryIndex) {
        if (m_QueryPool == VK_NULL_HANDLE) return;
        vkCmdWriteTimestamp(commandBuffer, pipelineStage, m_QueryPool, imageIndex * 2 + queryIndex);
    }

    void VulkanContext::FetchQueryResults(uint32_t imageIndex) {
        if (m_QueryPool == VK_NULL_HANDLE) return;

        uint64_t buffer[2];
        VkResult result = vkGetQueryPoolResults(
            m_Device,
            m_QueryPool,
            imageIndex * 2,
            2,
            sizeof(buffer),
            buffer,
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT
        );

        if (result == VK_SUCCESS) {
            m_GPUTime = (buffer[1] - buffer[0]) * m_TimestampPeriod / 1000000.0f;
        }
    }

    void VulkanContext::CleanupSwapchain() {
        vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
        vkDestroyImage(m_Device, m_DepthImage, nullptr);
        vkFreeMemory(m_Device, m_DepthImageMemory, nullptr);

        for (auto framebuffer : m_SwapchainFramebuffers) {
            vkDestroyFramebuffer(m_Device, framebuffer, nullptr);
        }
        m_SwapchainFramebuffers.clear();

        for (auto imageView : m_SwapchainImageViews) {
            vkDestroyImageView(m_Device, imageView, nullptr);
        }
        m_SwapchainImageViews.clear();

        if (m_Swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
            m_Swapchain = VK_NULL_HANDLE;
        }
    }

    void VulkanContext::RecreateSwapchain(SDL_Window* window) {
        int width = 0, height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        if (width == 0 || height == 0) {
            return;
        }

        vkDeviceWaitIdle(m_Device);

        CleanupSwapchain();

        CreateSwapchain(window);
        CreateImageViews();
        CreateDepthResources();
        CreateFramebuffers();
    }

    VkSurfaceFormatKHR VulkanContext::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
        for (const auto& availableFormat : availableFormats) {
            if ((availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB || availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB) && 
                 availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    VkPresentModeKHR VulkanContext::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
        for (const auto& availablePresentMode : availablePresentModes) {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D VulkanContext::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, SDL_Window* window) {
        if (capabilities.currentExtent.width != (std::numeric_limits<uint32_t>::max)()) {
            return capabilities.currentExtent;
        } else {
            int width, height;
            SDL_GetWindowSizeInPixels(window, &width, &height);
            VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
            return actualExtent;
        }
    }

    void VulkanContext::CreateCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = m_GraphicsQueueFamily;
        if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_CommandPool) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create command pool!");
        }
    }

    void VulkanContext::CreateCommandBuffers() {
        m_CommandBuffers.resize(m_SwapchainImages.size());
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)m_CommandBuffers.size();
        if (vkAllocateCommandBuffers(m_Device, &allocInfo, m_CommandBuffers.data()) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to allocate command buffers!");
        }
    }

    void VulkanContext::CreateDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(m_SwapchainImages.size());
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = 100; // Large enough for many textures

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = 200; // Support many sets

        if (vkCreateDescriptorPool(m_Device, &poolInfo, nullptr, &m_DescriptorPool) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create descriptor pool!");
        }
    }

    void VulkanContext::CreateTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

        if (vkCreateSampler(m_Device, &samplerInfo, nullptr, &m_TextureSampler) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create texture sampler!");
        }
    }

    void VulkanContext::CreateDescriptorSetLayout() {
        // Layout 0: Global UBO
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkDescriptorSetLayoutCreateInfo globalLayoutInfo{};
        globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        globalLayoutInfo.bindingCount = 1;
        globalLayoutInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(m_Device, &globalLayoutInfo, nullptr, &m_GlobalDescriptorSetLayout) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create global descriptor set layout!");
        }

        // Layout 1: Texture Sampler
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 0;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo textureLayoutInfo{};
        textureLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        textureLayoutInfo.bindingCount = 1;
        textureLayoutInfo.pBindings = &samplerLayoutBinding;

        if (vkCreateDescriptorSetLayout(m_Device, &textureLayoutInfo, nullptr, &m_TextureDescriptorSetLayout) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create texture descriptor set layout!");
        }

        // Push Constants
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(PushConstantData);

        std::array<VkDescriptorSetLayout, 2> layouts = { m_GlobalDescriptorSetLayout, m_TextureDescriptorSetLayout };
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(layouts.size());
        pipelineLayoutInfo.pSetLayouts = layouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(m_Device, &pipelineLayoutInfo, nullptr, &m_PipelineLayout) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create pipeline layout!");
        }

        // Upscale Layout
        VkDescriptorSetLayoutBinding upscaleBinding{};
        upscaleBinding.binding = 0;
        upscaleBinding.descriptorCount = 1;
        upscaleBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        upscaleBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo upscaleLayoutInfo{};
        upscaleLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        upscaleLayoutInfo.bindingCount = 1;
        upscaleLayoutInfo.pBindings = &upscaleBinding;

        if (vkCreateDescriptorSetLayout(m_Device, &upscaleLayoutInfo, nullptr, &m_UpscaleDescriptorSetLayout) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create upscale descriptor set layout!");
        }

        VkPipelineLayoutCreateInfo upscalePipelineLayoutInfo{};
        upscalePipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        upscalePipelineLayoutInfo.setLayoutCount = 1;
        upscalePipelineLayoutInfo.pSetLayouts = &m_UpscaleDescriptorSetLayout;

        if (vkCreatePipelineLayout(m_Device, &upscalePipelineLayoutInfo, nullptr, &m_UpscalePipelineLayout) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create upscale pipeline layout!");
        }
    }

    void VulkanContext::CreateUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(GlobalUBO);
        m_UniformBuffers.resize(m_SwapchainImages.size());
        for (size_t i = 0; i < m_SwapchainImages.size(); i++) {
            m_UniformBuffers[i] = std::make_unique<Buffer>(*this, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            m_UniformBuffers[i]->Map();
        }
    }

    void VulkanContext::CreateDefaultTexture() {
        uint32_t white = 0xFFFFFFFF;
        VkDeviceSize imageSize = 4;

        Buffer staging(*this, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        staging.Map();
        staging.WriteToBuffer(&white, imageSize);
        staging.Unmap();

        CreateImage(1, 1, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_DefaultImage, m_DefaultImageMemory);

        // Transition & Copy
        VkCommandBuffer commandBuffer = BeginSingleTimeCommands();
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_DefaultImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        region.imageExtent = { 1, 1, 1 };
        vkCmdCopyBufferToImage(commandBuffer, staging.GetBuffer(), m_DefaultImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        EndSingleTimeCommands(commandBuffer);

        m_DefaultImageView = CreateImageView(m_DefaultImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
        m_DefaultTextureDescriptorSet = CreateTextureDescriptorSet(m_DefaultImageView);
    }

    VkDescriptorSet VulkanContext::CreateTextureDescriptorSet(VkImageView imageView) {
        if (imageView == VK_NULL_HANDLE) {
            imageView = m_DefaultImageView;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &m_TextureDescriptorSetLayout;

        VkDescriptorSet descriptorSet;
        if (vkAllocateDescriptorSets(m_Device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = imageView;
        imageInfo.sampler = m_TextureSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);

        return descriptorSet;
    }

    void VulkanContext::CreateDescriptorSets() {
        // Global UBO sets
        std::vector<VkDescriptorSetLayout> layouts(m_SwapchainImages.size(), m_GlobalDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = m_DescriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(m_SwapchainImages.size());
        allocInfo.pSetLayouts = layouts.data();

        m_GlobalDescriptorSets.resize(m_SwapchainImages.size());
        vkAllocateDescriptorSets(m_Device, &allocInfo, m_GlobalDescriptorSets.data());

        for (size_t i = 0; i < m_SwapchainImages.size(); i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = m_UniformBuffers[i]->GetBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(GlobalUBO);

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_GlobalDescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &bufferInfo;

            vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
        }

        // Upscale sets
        std::vector<VkDescriptorSetLayout> upscaleLayouts(m_SwapchainImages.size(), m_UpscaleDescriptorSetLayout);
        VkDescriptorSetAllocateInfo upscaleAllocInfo{};
        upscaleAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        upscaleAllocInfo.descriptorPool = m_DescriptorPool;
        upscaleAllocInfo.descriptorSetCount = static_cast<uint32_t>(m_SwapchainImages.size());
        upscaleAllocInfo.pSetLayouts = upscaleLayouts.data();

        m_UpscaleDescriptorSets.resize(m_SwapchainImages.size());
        vkAllocateDescriptorSets(m_Device, &upscaleAllocInfo, m_UpscaleDescriptorSets.data());
    }

    void VulkanContext::UpdateUpscaleDescriptorSets(VkImageView colorImageView) {
        for (size_t i = 0; i < m_SwapchainImages.size(); i++) {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = colorImageView;
            imageInfo.sampler = m_TextureSampler;

            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = m_UpscaleDescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &imageInfo;

            vkUpdateDescriptorSets(m_Device, 1, &descriptorWrite, 0, nullptr);
        }
    }

    VkResult VulkanContext::AcquireNextImage(VkSemaphore signalSemaphore, uint32_t* imageIndex) {
        return vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, signalSemaphore, VK_NULL_HANDLE, imageIndex);
    }

    void VulkanContext::SubmitCommandBuffer(uint32_t imageIndex, VkSemaphore waitSemaphore, VkSemaphore signalSemaphore, VkFence fence) {
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { waitSemaphore };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &m_CommandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = { signalSemaphore };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, fence);
    }

    VkResult VulkanContext::PresentImage(uint32_t imageIndex, VkSemaphore waitSemaphore) {
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        VkSemaphore waitSemaphores[] = { waitSemaphore };
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = waitSemaphores;
        VkSwapchainKHR swapchains[] = { m_Swapchain };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        return vkQueuePresentKHR(m_GraphicsQueue, &presentInfo);
    }

    uint32_t VulkanContext::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(m_PhysicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) return i;
        }
        return 0;
    }

    VkFormat VulkanContext::FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (VkFormat format : candidates) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &props);
            if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) return format;
            else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) return format;
        }
        return VK_FORMAT_UNDEFINED;
    }

    VkFormat VulkanContext::FindDepthFormat() {
        return FindSupportedFormat({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void VulkanContext::CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = { width, height, 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        vkCreateImage(m_Device, &imageInfo, nullptr, &image);
        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(m_Device, image, &memReqs);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, properties);
        vkAllocateMemory(m_Device, &allocInfo, nullptr, &imageMemory);
        vkBindImageMemory(m_Device, image, imageMemory, 0);
    }

    VkImageView VulkanContext::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = { aspectFlags, 0, 1, 0, 1 };
        VkImageView imageView;
        vkCreateImageView(m_Device, &viewInfo, nullptr, &imageView);
        return imageView;
    }

    VkCommandBuffer VulkanContext::BeginSingleTimeCommands() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = m_CommandPool;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(m_Device, &allocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void VulkanContext::EndSingleTimeCommands(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_GraphicsQueue);

        vkFreeCommandBuffers(m_Device, m_CommandPool, 1, &commandBuffer);
    }

    bool VulkanContext::CheckValidationLayerSupport() {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
        for (const char* layerName : m_ValidationLayers) {
            bool layerFound = false;
            for (const auto& layerProperties : availableLayers) {
                if (strcmp(layerName, layerProperties.layerName) == 0) { layerFound = true; break; }
            }
            if (!layerFound) return false;
        }
        return true;
    }

    std::vector<const char*> VulkanContext::GetRequiredExtensions() {
        uint32_t sdlExtCount = 0;
        const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
        std::vector<const char*> exts(sdlExts, sdlExts + sdlExtCount);
        if (m_EnableValidationLayers) exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        return exts;
    }

}
