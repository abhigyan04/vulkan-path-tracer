#pragma once

#include <vulkan/vulkan.h>
#include "sbt.hpp"


void recordCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, uint32_t imageIndex, VkPipeline pipeline,
    VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
    VkImage rtImage, std::vector<VkImage> swapChainImages, const SBT& sbt, VkExtent2D extent);
