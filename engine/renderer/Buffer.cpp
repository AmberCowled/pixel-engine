#include "Buffer.hpp"
#include "VulkanContext.hpp"
#include <engine/base/Log.hpp>
#include <cstring>

namespace PixelEngine {

    Buffer::Buffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
        : m_Context(context) {
        
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_Context.GetDevice(), &bufferInfo, nullptr, &m_Buffer) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to create buffer!");
        }

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(m_Context.GetDevice(), m_Buffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = m_Context.FindMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(m_Context.GetDevice(), &allocInfo, nullptr, &m_Memory) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to allocate buffer memory!");
        }

        vkBindBufferMemory(m_Context.GetDevice(), m_Buffer, m_Memory, 0);
    }

    Buffer::~Buffer() {
        if (m_Mapped) {
            Unmap();
        }
        vkDestroyBuffer(m_Context.GetDevice(), m_Buffer, nullptr);
        vkFreeMemory(m_Context.GetDevice(), m_Memory, nullptr);
    }

    void Buffer::Map(VkDeviceSize size, VkDeviceSize offset) {
        if (vkMapMemory(m_Context.GetDevice(), m_Memory, offset, size, 0, &m_Mapped) != VK_SUCCESS) {
            PX_CORE_CRITICAL("Failed to map buffer memory!");
        }
    }

    void Buffer::Unmap() {
        vkUnmapMemory(m_Context.GetDevice(), m_Memory);
        m_Mapped = nullptr;
    }

    void Buffer::WriteToBuffer(void* data, VkDeviceSize size, VkDeviceSize offset) {
        if (size == VK_WHOLE_SIZE) {
            // This is simplified, in a real scenario we'd need to know the buffer size
            // For PoC we assume the caller provides correct size or we map whole size.
        }
        std::memcpy((uint8_t*)m_Mapped + offset, data, size);
    }

}
