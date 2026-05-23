#pragma once

#include <vulkan/vulkan.h>

namespace PixelEngine {

    class VulkanContext;

    class Buffer {
    public:
        Buffer(VulkanContext& context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
        ~Buffer();

        void Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        void Unmap();

        void WriteToBuffer(void* data, VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
        
                VkBuffer GetBuffer() const { return m_Buffer; }
        VkDeviceMemory GetMemory() const { return m_Memory; }
        void* GetMapped() const { return m_Mapped; }

    private:
        VulkanContext& m_Context;
        VkBuffer m_Buffer = VK_NULL_HANDLE;
        VkDeviceMemory m_Memory = VK_NULL_HANDLE;
        void* m_Mapped = nullptr;
    };

}
