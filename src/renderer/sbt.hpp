#pragma once

#include <vulkan/vulkan.h>
#include "gpuScene.hpp"


struct SBT
{
    VkBuffer buffer;
    VkDeviceMemory memory;

    VkStridedDeviceAddressRegionKHR raygenRegion;
    VkStridedDeviceAddressRegionKHR missRegion;
    VkStridedDeviceAddressRegionKHR hitRegion;
    VkStridedDeviceAddressRegionKHR callableRegion;
};

SBT createSBT(VkDevice device, VkPhysicalDevice physicalDevice, VkPipeline rayTracingPipeline);
void destroySBT(VkDevice device, SBT& s);