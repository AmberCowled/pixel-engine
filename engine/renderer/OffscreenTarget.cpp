#include "OffscreenTarget.hpp"
#include "VulkanContext.hpp"
#include <engine/base/Log.hpp>
#include <array>

namespace PixelEngine {

    OffscreenTarget::OffscreenTarget(VulkanContext& context, const OffscreenTargetConfig& config, VkRenderPass renderPass)
        : m_Context(context), m_Width(config.width), m_Height(config.height) {
        
        // Color Resource
        m_Context.CreateImage(
            m_Width, m_Height, 
            config.colorFormat, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            m_ColorImage, m_ColorMemory
        );
        m_ColorView = m_Context.CreateImageView(m_ColorImage, config.colorFormat, VK_IMAGE_ASPECT_COLOR_BIT);

        // Depth Resource
        m_Context.CreateImage(
            m_Width, m_Height, 
            config.depthFormat, 
            VK_IMAGE_TILING_OPTIMAL, 
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, 
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
            m_DepthImage, m_DepthMemory
        );
        m_DepthView = m_Context.CreateImageView(m_DepthImage, config.depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

        // Framebuffer
        std::array<VkImageView, 2> attachments = { m_ColorView, m_DepthView };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_Width;
        framebufferInfo.height = m_Height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_Context.GetDevice(), &framebufferInfo, nullptr, &m_Framebuffer) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create offscreen framebuffer!");
        }
    }

    OffscreenTarget::~OffscreenTarget() {
        vkDestroyFramebuffer(m_Context.GetDevice(), m_Framebuffer, nullptr);
        
        vkDestroyImageView(m_Context.GetDevice(), m_ColorView, nullptr);
        vkDestroyImage(m_Context.GetDevice(), m_ColorImage, nullptr);
        vkFreeMemory(m_Context.GetDevice(), m_ColorMemory, nullptr);

        vkDestroyImageView(m_Context.GetDevice(), m_DepthView, nullptr);
        vkDestroyImage(m_Context.GetDevice(), m_DepthImage, nullptr);
        vkFreeMemory(m_Context.GetDevice(), m_DepthMemory, nullptr);
    }

}
