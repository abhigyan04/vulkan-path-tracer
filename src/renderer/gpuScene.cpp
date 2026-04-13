#include "gpuScene.hpp"
#include <iostream>

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{};
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlagsInfo.flags = 0;

    //Required for SBT and AS later
    if(usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if(allocFlagsInfo.flags != 0) {
        allocInfo.pNext = &allocFlagsInfo;
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo bufferDeviceAI{};
    bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAI.buffer = buffer;
    return vkGetBufferDeviceAddress(device, &bufferDeviceAI);
}

//Begin single time command buffer helper function
VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer VkCommandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &VkCommandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(VkCommandBuffer, &beginInfo);

    return VkCommandBuffer;
}

//End single time command buffer helper function
void endSingleTimeCommands(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

//Copy buffer helper function
void copyBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void createImage(VkDevice device, VkPhysicalDevice physicalDevice, int width, int height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& memory)
{
    VkImageCreateInfo rtImageCreateInfo{};
    rtImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    rtImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    rtImageCreateInfo.extent.width = width;
    rtImageCreateInfo.extent.height = height;
    rtImageCreateInfo.extent.depth = 1;
    rtImageCreateInfo.mipLevels = 1;
    rtImageCreateInfo.arrayLayers = 1;
    rtImageCreateInfo.format = format;
    rtImageCreateInfo.tiling = tiling;
    rtImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    rtImageCreateInfo.usage = usage;
    rtImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    rtImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &rtImageCreateInfo, nullptr, &image) != VK_SUCCESS) {
        std::cerr << "Failed to create ray tracing output image!" << std::endl;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo memoryAllocInfo{};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memRequirements.size;
    memoryAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &memoryAllocInfo, nullptr, &memory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory for ray tracing output image!" << std::endl;
    }

    vkBindImageMemory(device, image, memory, 0);

    std::cout<<"Texture image created successfully! \n";
}

SceneGPUResources uploadSceneToGPU(VkDevice device, VkPhysicalDevice physicalDevice, const SceneData& scene)
{
    SceneGPUResources resources{};

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        sizeof(GPUVertex) * scene.mesh.vertices.size(),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        vertexBuffer,
        vertexBufferMemory
    );

    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, sizeof(GPUVertex) * scene.mesh.vertices.size(), 0, &data);
    memcpy(data, scene.mesh.vertices.data(), (size_t)(sizeof(GPUVertex) * scene.mesh.vertices.size()));
    vkUnmapMemory(device, vertexBufferMemory);

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        sizeof(uint32_t) * scene.mesh.indices.size(),
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        indexBuffer,
        indexBufferMemory
    );

    vkMapMemory(device, indexBufferMemory, 0, sizeof(uint32_t) * scene.mesh.indices.size(), 0, &data);
    memcpy(data, scene.mesh.indices.data(), (size_t)(sizeof(uint32_t) * scene.mesh.indices.size()));
    vkUnmapMemory(device, indexBufferMemory);

    VkBuffer materialBuffer;
    VkDeviceMemory materialBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        sizeof(GPUMaterial) * scene.materials.size(),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        materialBuffer,
        materialBufferMemory
    );

    vkMapMemory(device, materialBufferMemory, 0, sizeof(GPUMaterial) * scene.materials.size(), 0, &data);
    memcpy(data, scene.materials.data(), (size_t)(sizeof(GPUMaterial) * scene.materials.size()));
    vkUnmapMemory(device, materialBufferMemory);

    resources.vertexBuffer = vertexBuffer;
    resources.vertexBufferMemory = vertexBufferMemory;
    resources.indexBuffer = indexBuffer;
    resources.indexBufferMemory = indexBufferMemory;
    resources.materialBuffer = materialBuffer;
    resources.materialBufferMemory = materialBufferMemory;

    return resources;
}

void destroySceneGPU(VkDevice device, SceneGPUResources& r, std::vector<GPUTexture>& textures)
{
    vkDestroyBuffer(device, r.vertexBuffer, nullptr);
    vkFreeMemory(device, r.vertexBufferMemory, nullptr);

    vkDestroyBuffer(device, r.indexBuffer, nullptr);
    vkFreeMemory(device, r.indexBufferMemory, nullptr);

    vkDestroyBuffer(device, r.materialBuffer, nullptr);
    vkFreeMemory(device, r.materialBufferMemory, nullptr);

    for (auto& tex : textures)
    {
        tex.destroy(device);
    }
}
