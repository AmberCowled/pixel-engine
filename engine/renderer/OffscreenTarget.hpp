#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace PixelEngine {

    class VulkanContext;

    struct OffscreenTargetConfig {
        uint32_t width;
        uint32_t height;
        VkFormat colorFormat;
        VkFormat depthFormat;
    };

    class OffscreenTarget {
    public:
        OffscreenTarget(VulkanContext& context, const OffscreenTargetConfig& config, VkRenderPass renderPass);
        ~OffscreenTarget();

        VkFramebuffer GetFramebuffer() const { return m_Framebuffer; }
        VkImageView GetColorImageView() const { return m_ColorView; }
        VkImage GetColorImage() const { return m_ColorImage; }
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

    private:
        VulkanContext& m_Context;
        uint32_t m_Width;
        uint32_t m_Height;

        VkImage m_ColorImage = VK_NULL_HANDLE;
        VkDeviceMemory m_ColorMemory = VK_NULL_HANDLE;
        VkImageView m_ColorView = VK_NULL_HANDLE;

        VkImage m_DepthImage = VK_NULL_HANDLE;
        VkDeviceMemory m_DepthMemory = VK_NULL_HANDLE;
        VkImageView m_DepthView = VK_NULL_HANDLE;

        VkFramebuffer m_Framebuffer = VK_NULL_HANDLE;
    };

}
