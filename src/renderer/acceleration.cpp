#include "acceleration.hpp"
#include <vulkan/vulkan.h>
#include <iostream>


AccelerationStructures buildAccelerationStructures(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const SceneData& scene, const SceneGPUResources& gpuScene)
{
    AccelerationStructures as{};

    auto vkCreateAccelerationStructureKHR =
        (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    auto vkCmdBuildAccelerationStructuresKHR =
        (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    auto vkGetAccelerationStructureDeviceAddressKHR =
        (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");
    auto vkGetAccelerationStructureBuildSizesKHR =
        (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");

    //Build BLAS
    VkDeviceAddress vertexBufferAddress = getBufferDeviceAddress(device, gpuScene.vertexBuffer);
    VkDeviceAddress indexBufferAddress = getBufferDeviceAddress(device, gpuScene.indexBuffer);

    VkAccelerationStructureGeometryTrianglesDataKHR trianglesData{};
    trianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    trianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    trianglesData.vertexStride = sizeof(GPUVertex);
    trianglesData.vertexData.deviceAddress = vertexBufferAddress;
    trianglesData.maxVertex = static_cast<uint32_t>(scene.mesh.vertices.size());
    
    trianglesData.indexType = VK_INDEX_TYPE_UINT32;
    trianglesData.indexData.deviceAddress = indexBufferAddress;
    trianglesData.transformData.deviceAddress = 0;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = trianglesData;

    uint32_t primitiveCount = static_cast<uint32_t>(scene.mesh.indices.size() / 3);

    //Query sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );

    //Create BLAS buffer
    VkBuffer blasBuffer;
    VkDeviceMemory blasMemory;

    createBuffer(
        device,
        physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blasBuffer,
        blasMemory
    );

    VkAccelerationStructureKHR blas;
    VkAccelerationStructureCreateInfoKHR asCreateInfo{};
    asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asCreateInfo.buffer = blasBuffer;
    asCreateInfo.size = sizeInfo.accelerationStructureSize;
    asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    vkCreateAccelerationStructureKHR(device, &asCreateInfo, nullptr, &blas);

    //Create scratch buffer
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;

    createBuffer(
        device,
        physicalDevice,
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer,
        scratchMemory
    );

    VkDeviceAddress scratchAddress = 
        getBufferDeviceAddress(device, scratchBuffer);

    //Finalize build info
    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    //Record command buffer to build BLAS
    VkCommandBuffer buildCmdBuffer = beginSingleTimeCommands(device, commandPool);
    
    vkCmdBuildAccelerationStructuresKHR(
        buildCmdBuffer,
        1,
        &buildInfo,
        &pRangeInfo
    );

    // Barrier: build -> usable by ray tracing
    VkMemoryBarrier blasBarrier{};
    blasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    blasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    blasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        buildCmdBuffer,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &blasBarrier,
        0, nullptr,
        0, nullptr
    );

    endSingleTimeCommands(device, commandPool, graphicsQueue, buildCmdBuffer);

    vkDestroyBuffer(device, scratchBuffer, nullptr);
    vkFreeMemory(device, scratchMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = blas;

    VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);
    std::cout << "BLAS address: " << blasAddr << "\n";

    //Build TLAS
    //Create instance struct for TLAS
    VkAccelerationStructureInstanceKHR accelStructureInstance{};

    accelStructureInstance.transform = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0
    };

    accelStructureInstance.instanceCustomIndex = 0;
    accelStructureInstance.mask = 0xFF;
    accelStructureInstance.instanceShaderBindingTableRecordOffset = 0;
    accelStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    accelStructureInstance.accelerationStructureReference = blasAddr;

    //Create staging buffer
    VkDeviceSize accelBufferSize = sizeof(accelStructureInstance) * 1; // One instance
    
    VkBuffer accelStagingBuffer;
    VkDeviceMemory accelStagingBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        accelBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        accelStagingBuffer,
        accelStagingBufferMemory
    );

    //Copy instance data to staging buffer
    void* accelData;
    vkMapMemory(device, accelStagingBufferMemory, 0, accelBufferSize, 0, &accelData);
    memcpy(accelData, &accelStructureInstance, (size_t)accelBufferSize);
    vkUnmapMemory(device, accelStagingBufferMemory);

    //Create GPU buffer for TLAS instances
    VkBuffer accelBuffer;
    VkDeviceMemory accelBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        accelBufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        accelBuffer,
        accelBufferMemory);

    //Copy data from staging buffer to GPU buffer
    copyBuffer(device, commandPool, graphicsQueue, accelStagingBuffer, accelBuffer, accelBufferSize);
    vkDestroyBuffer(device, accelStagingBuffer, nullptr);
    vkFreeMemory(device, accelStagingBufferMemory, nullptr);

    //Describe TLAS geometry
    VkDeviceAddress accelBufferAddress = getBufferDeviceAddress(device, accelBuffer);

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;

    tlasGeometry.geometry.instances.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

    tlasGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
    tlasGeometry.geometry.instances.data.deviceAddress = accelBufferAddress;
    tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    //Query sizes
    VkAccelerationStructureBuildGeometryInfoKHR accelBuildInfo{};
    accelBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    accelBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelBuildInfo.geometryCount = 1;
    accelBuildInfo.pGeometries = &tlasGeometry;
    accelBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    uint32_t accelPrimitiveCount = 1; // One instance

    VkAccelerationStructureBuildSizesInfoKHR accelSizeInfo{};
    accelSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelBuildInfo,
        &accelPrimitiveCount,
        &accelSizeInfo
    );

    //Create TLAS buffer
    VkBuffer tlasBuffer;
    VkDeviceMemory tlasMemory;

    createBuffer(
        device,
        physicalDevice,
        accelSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBuffer,
        tlasMemory
    );

    VkAccelerationStructureKHR tlas;
    VkAccelerationStructureCreateInfoKHR tlasAsCreateInfo{};
    tlasAsCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasAsCreateInfo.buffer = tlasBuffer;
    tlasAsCreateInfo.size = accelSizeInfo.accelerationStructureSize;
    tlasAsCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    vkCreateAccelerationStructureKHR(device, &tlasAsCreateInfo, nullptr, &tlas);

    //Create scratch buffer
    VkBuffer accelScratchBuffer;
    VkDeviceMemory accelScratchMemory;

    createBuffer(
        device,
        physicalDevice,
        accelSizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        accelScratchBuffer,
        accelScratchMemory
    );

    VkDeviceAddress accelScratchAddress = 
        getBufferDeviceAddress(device, accelScratchBuffer);

    //Finalize build info
    accelBuildInfo.dstAccelerationStructure = tlas;
    accelBuildInfo.scratchData.deviceAddress = accelScratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR accelRangeInfo{};
    accelRangeInfo.primitiveCount = accelPrimitiveCount;

    const VkAccelerationStructureBuildRangeInfoKHR* accelRanges[] = { &accelRangeInfo };

    //Record command buffer to build TLAS
    VkCommandBuffer accelBuildCmdBuffer = beginSingleTimeCommands(device, commandPool);
    
    vkCmdBuildAccelerationStructuresKHR(
        accelBuildCmdBuffer,
        1,
        &accelBuildInfo,
        accelRanges
    );

    // Barrier: build -> usable by ray tracing
    VkMemoryBarrier accelBarrier{};
    accelBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    accelBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    accelBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        accelBuildCmdBuffer,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &accelBarrier,
        0, nullptr,
        0, nullptr
    );

    endSingleTimeCommands(device, commandPool, graphicsQueue, accelBuildCmdBuffer);

    vkDestroyBuffer(device, accelScratchBuffer, nullptr);
    vkFreeMemory(device, accelScratchMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR accelAddrInfo{};
    accelAddrInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    accelAddrInfo.accelerationStructure = tlas;

    VkDeviceAddress tlasAddr =
        vkGetAccelerationStructureDeviceAddressKHR(device, &accelAddrInfo);

    std::cout << "TLAS address: " << tlasAddr << "\n";

    as.blas = blas;
    as.tlas = tlas;
    as.tlasAddress = tlasAddr;
    as.blasBuffer = blasBuffer;
    as.blasMemory = blasMemory;
    as.tlasBuffer = tlasBuffer;
    as.tlasMemory = tlasMemory;
    as.accelBuffer = accelBuffer;
    as.accelBufferMemory = accelBufferMemory;

    return as;
}

void destroyAccelerationStructures(VkDevice device, AccelerationStructures& a)
{
    auto vkDestroyAccelerationStructureKHR =
        (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
    
    vkDestroyAccelerationStructureKHR(device, a.tlas, nullptr);
    vkDestroyBuffer(device, a.tlasBuffer, nullptr);
    vkFreeMemory(device, a.tlasMemory, nullptr);

    vkDestroyAccelerationStructureKHR(device, a.blas, nullptr);
    vkDestroyBuffer(device, a.blasBuffer, nullptr);
    vkFreeMemory(device, a.blasMemory, nullptr);

    vkDestroyBuffer(device, a.accelBuffer, nullptr);
    vkFreeMemory(device, a.accelBufferMemory, nullptr);
}
