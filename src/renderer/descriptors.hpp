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

DescriptorBundle createSceneDescriptorSet(VkDevice device, VkAccelerationStructureKHR tlas, VkBuffer cameraBuffer, const SceneGPUResources& resources, const std::vector<GPUTexture>& textures);
void updateSceneDescriptors(VkDevice device, VkDescriptorSet descriptorSet, const SceneGPUResources& resources,
    VkAccelerationStructureKHR tlas, VkImageView rtImageView, VkImageView accumImageView, VkBuffer cameraBuffer, const std::vector<GPUTexture>& textures);
void destroyDescriptors(VkDevice device, DescriptorBundle& d);
