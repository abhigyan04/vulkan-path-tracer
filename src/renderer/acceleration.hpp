#pragma once

#include <vulkan/vulkan.h>
#include "gpuScene.hpp"
#include "scene.hpp"


struct AccelerationStructures
{
    VkAccelerationStructureKHR blas;
    VkAccelerationStructureKHR tlas;
    VkDeviceAddress tlasAddress;
    VkBuffer blasBuffer;
    VkDeviceMemory blasMemory;
    VkBuffer tlasBuffer;
    VkDeviceMemory tlasMemory;
    VkBuffer accelBuffer;
    VkDeviceMemory accelBufferMemory;
};

AccelerationStructures buildAccelerationStructures(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const SceneData& scene, const SceneGPUResources& gpuScene);
void destroyAccelerationStructures(VkDevice device, AccelerationStructures& a);
