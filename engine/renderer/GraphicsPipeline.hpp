#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <vector>

namespace PixelEngine {

    class VulkanContext;

    struct PipelineConfigInfo {
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    class GraphicsPipeline {
    public:
        GraphicsPipeline(
            VulkanContext& context, 
            const std::string& vertFilePath, 
            const std::string& fragFilePath, 
            const PipelineConfigInfo& configInfo);
        ~GraphicsPipeline();

        void Bind(VkCommandBuffer commandBuffer);

        static void DefaultPipelineConfigInfo(PipelineConfigInfo& configInfo);

    private:
        static std::vector<char> ReadFile(const std::string& filePath);
        void CreateGraphicsPipeline(
            const std::string& vertFilePath, 
            const std::string& fragFilePath, 
            const PipelineConfigInfo& configInfo);
        
        VkShaderModule CreateShaderModule(const std::vector<char>& code);

    private:
        VulkanContext& m_Context;
        VkPipeline m_GraphicsPipeline;
        VkShaderModule m_VertShaderModule;
        VkShaderModule m_FragShaderModule;
    };

}
