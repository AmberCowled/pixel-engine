#pragma once

#include <vulkan/vulkan.h>
#include <string>

namespace PixelEngine {

    class VulkanContext;

    class Texture {
    public:
        Texture(VulkanContext& context, const std::string& path);
        ~Texture();

        VkImageView GetImageView() const { return m_TextureImageView; }
        
        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

    private:
        void CreateTextureImage(const std::string& path);
        void CreateTextureImageView();

        void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        void CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

    private:
        VulkanContext& m_Context;
        uint32_t m_Width, m_Height;

        VkImage m_TextureImage = VK_NULL_HANDLE;
        VkDeviceMemory m_TextureImageMemory = VK_NULL_HANDLE;
        VkImageView m_TextureImageView = VK_NULL_HANDLE;
    };

}
