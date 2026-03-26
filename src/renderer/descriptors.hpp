#pragma once

#include <vulkan/vulkan.h>
#include "gpuScene.hpp"
#include "camera.hpp"

struct DescriptorBundle
{
    VkDescriptorSetLayout layout;
    VkDescriptorSet set;
    VkDescriptorPool pool;
};

DescriptorBundle createSceneDescriptorSet(VkDevice device, VkAccelerationStructureKHR tlas, VkImageView outputImageView, VkBuffer cameraBuffer, const SceneGPUResources& resources);
void updateSceneDescriptors(VkDevice device, VkDescriptorSet descriptorSet, const SceneGPUResources& resources,
    VkAccelerationStructureKHR tlas, VkImageView rtImageView, VkBuffer cameraBuffer);
void destroyDescriptors(VkDevice device, DescriptorBundle& d);