#pragma once

#include <vulkan/vulkan.h>
#include "scene.hpp"


struct SceneGPUResources
{
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkBuffer materialBuffer;
    VkDeviceMemory materialBufferMemory;

    VkBuffer materialIDBuffer;
    VkDeviceMemory materialIDBufferMemory;
};

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer);
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
SceneGPUResources uploadSceneToGPU(VkDevice device, VkPhysicalDevice physicalDevice, const SceneData& scene);
void endSingleTimeCommands(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer commandBuffer);
VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);
void copyBuffer(VkDevice device, VkCommandPool commandPool, VkQueue graphicsQueue, VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
void destroySceneGPU(VkDevice device, SceneGPUResources& r);
