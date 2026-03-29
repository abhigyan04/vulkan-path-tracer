#pragma once

#include <vulkan/vulkan.h>
#include "sbt.hpp"
#include "pipeline.hpp"


void recordCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t imageIndex, VkPipeline pipeline,
    VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
    VkImage rtImage, VkImage accumImage, std::vector<VkImage> swapChainImages, const SBT& sbt, VkExtent2D extent, bool cameraMoved);
