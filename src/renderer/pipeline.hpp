#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>


struct RTPipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

std::vector<char> readFile(const std::string& filename);
VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code);
RTPipeline createRayTracingPipeline(VkDevice device, VkDescriptorSetLayout descriptorSetLayout);
void destroyPipeline(VkDevice device, RTPipeline& p);