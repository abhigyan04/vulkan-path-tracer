#include "sbt.hpp"
#include <vector>
#include <iostream>


SBT createSBT(VkDevice device, VkPhysicalDevice physicalDevice, VkPipeline rayTracingPipeline)
{
    auto vkGetRayTracingShaderGroupHandlesKHR =
        (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    
    SBT sbt;

    //Build the SBT
    const uint32_t groupCount = 4;
    

    auto alignUp = [](VkDeviceSize v, VkDeviceSize a) {
        return (v + a - 1) & ~(a - 1);
    };

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};
    rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 deviceProperties2{};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &rayTracingPipelineProperties;

    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

    VkDeviceSize handleSize   = rayTracingPipelineProperties.shaderGroupHandleSize;
    VkDeviceSize handleAlign  = rayTracingPipelineProperties.shaderGroupHandleAlignment;
    VkDeviceSize baseAlign    = rayTracingPipelineProperties.shaderGroupBaseAlignment;

    // Each record stride must be aligned to handle alignment
    VkDeviceSize recordStride = alignUp(handleSize, handleAlign);

    // Each region start must be aligned to base alignment
    VkDeviceSize raygenRegionSize = alignUp(recordStride, baseAlign);
    VkDeviceSize missRegionSize   = alignUp(recordStride * 2, baseAlign);
    VkDeviceSize hitRegionSize    = alignUp(recordStride, baseAlign);

    VkDeviceSize sbtSize = raygenRegionSize + missRegionSize + hitRegionSize;

    //Alocate storage for shader group handles
    std::vector<uint8_t> handles(groupCount * handleSize);

    if (vkGetRayTracingShaderGroupHandlesKHR(
        device,
        rayTracingPipeline,
        0,
        groupCount,
        handles.size(),
        handles.data()) != VK_SUCCESS)
    {
        std::cerr << "Failed to get ray tracing shader group handles!" << std::endl;
        SBT emptySBT{};
        return emptySBT;
    }

    //Create SBT buffer
    VkBuffer sbtBuffer;
    VkDeviceMemory sbtBufferMemory;

    createBuffer(device, physicalDevice, sbtSize,
    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    sbtBuffer, sbtBufferMemory);

    //Wrtite shader group handles to SBT buffer
    uint8_t* mapped = nullptr;
    vkMapMemory(device, sbtBufferMemory, 0, sbtSize, 0, (void**)&mapped);

    // group 0 = raygen
    memcpy(mapped + 0,
       handles.data() + 0 * handleSize,
       handleSize);

    // group 1 = miss
    memcpy(mapped + raygenRegionSize + 0 * recordStride,
       handles.data() + 1 * handleSize,
       handleSize);
    
    // group 4 = shadow miss
    memcpy(mapped + raygenRegionSize + 1 * recordStride,
       handles.data() + 3 * handleSize,
       handleSize);

    // group 2 = hit
    memcpy(mapped + raygenRegionSize + missRegionSize,
       handles.data() + 2 * handleSize,
       handleSize);

    vkUnmapMemory(device, sbtBufferMemory);

    //Create SBT buffer regions
    VkDeviceAddress sbtAddress = getBufferDeviceAddress(device, sbtBuffer);

    VkStridedDeviceAddressRegionKHR raygenSBT{};
    raygenSBT.deviceAddress = sbtAddress;
    raygenSBT.stride = recordStride;
    raygenSBT.size   = recordStride;
    
    VkStridedDeviceAddressRegionKHR missSBT{};
    missSBT.deviceAddress = sbtAddress + raygenRegionSize;
    missSBT.stride = recordStride;
    missSBT.size   = recordStride * 2;

    VkStridedDeviceAddressRegionKHR hitSBT{};
    hitSBT.deviceAddress = sbtAddress + raygenRegionSize + missRegionSize;
    hitSBT.stride = recordStride;
    hitSBT.size   = recordStride;

    VkStridedDeviceAddressRegionKHR callableSBT{};

    callableSBT.deviceAddress = 0;
    callableSBT.stride = 0;
    callableSBT.size = 0;

    sbt.buffer = sbtBuffer;
    sbt.memory = sbtBufferMemory;
    sbt.raygenRegion = raygenSBT;
    sbt.missRegion = missSBT;
    sbt.hitRegion = hitSBT;
    sbt.callableRegion = callableSBT;

    return sbt;
}

void destroySBT(VkDevice device, SBT& s)
{
    vkDestroyBuffer(device, s.buffer, nullptr);
    vkFreeMemory(device, s.memory, nullptr);
}
