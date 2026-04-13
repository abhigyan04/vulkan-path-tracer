#include "descriptors.hpp"
#include <array>


DescriptorBundle createSceneDescriptorSet(VkDevice device, VkAccelerationStructureKHR tlas, VkBuffer cameraBuffer, const SceneGPUResources& resources, const std::vector<GPUTexture>& textures)
{
    DescriptorBundle db;

    //Descriptor set for TLAS + output storage image
    VkDescriptorSetLayout tlasDescriptorSetLayout;

    VkDescriptorSetLayoutBinding asBinding{};
    asBinding.binding = 0;
    asBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    asBinding.descriptorCount = 1;
    asBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding imgBinding{};
    imgBinding.binding = 1;
    imgBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imgBinding.descriptorCount = 1;
    imgBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding cameraBufferBinding{};
    cameraBufferBinding.binding = 2;
    cameraBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBufferBinding.descriptorCount = 1;
    cameraBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding vertexBufferBinding{};
    vertexBufferBinding.binding = 3;
    vertexBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBufferBinding.descriptorCount = 1;
    vertexBufferBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding indexBufferBinding{};
    indexBufferBinding.binding = 4;
    indexBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexBufferBinding.descriptorCount = 1;
    indexBufferBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding materialBufferBinding{};
    materialBufferBinding.binding = 5;
    materialBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialBufferBinding.descriptorCount = 1;
    materialBufferBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding accumImgBinding{};
    accumImgBinding.binding = 6;
    accumImgBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    accumImgBinding.descriptorCount = 1;
    accumImgBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    const uint32_t maxTextureCount = 64;

    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 7;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = maxTextureCount; // Avoid zero descriptor count
    textureBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    std::array<VkDescriptorSetLayoutBinding, 8> bindings = { asBinding, imgBinding, cameraBufferBinding, vertexBufferBinding, indexBufferBinding, materialBufferBinding, accumImgBinding, textureBinding };

    VkDescriptorSetLayoutCreateInfo tlasLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    tlasLayoutInfo.bindingCount = (uint32_t)bindings.size();
    tlasLayoutInfo.pBindings = bindings.data();

    vkCreateDescriptorSetLayout(device, &tlasLayoutInfo, nullptr, &tlasDescriptorSetLayout);

    //Descriptor Pool and Descriptor Set for storage image
    VkDescriptorPool tlasDescriptorPool;
    VkDescriptorSet tlasDescriptorSet;

    std::array<VkDescriptorPoolSize, 8> poolSizes{};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
    poolSizes[2] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };
    poolSizes[3] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
    poolSizes[4] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
    poolSizes[5] = { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 };
    poolSizes[6] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
    poolSizes[7] = { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textures.size() > 0 ? static_cast<uint32_t>(textures.size()) : 1 };

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &poolInfo, nullptr, &tlasDescriptorPool);

    VkDescriptorSetAllocateInfo tlasAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    tlasAllocInfo.descriptorPool = tlasDescriptorPool;
    tlasAllocInfo.descriptorSetCount = 1;
    tlasAllocInfo.pSetLayouts = &tlasDescriptorSetLayout;

    vkAllocateDescriptorSets(device, &tlasAllocInfo, &tlasDescriptorSet);

    db.layout = tlasDescriptorSetLayout;
    db.set = tlasDescriptorSet;
    db.pool = tlasDescriptorPool;

    return db;
}

void updateSceneDescriptors(VkDevice device, VkDescriptorSet descriptorSet, const SceneGPUResources& resources, VkAccelerationStructureKHR tlas, VkImageView rtImageView, VkImageView accumImageView, VkBuffer cameraBuffer, const std::vector<GPUTexture>& textures)
{
    VkWriteDescriptorSetAccelerationStructureKHR asInfo{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &tlas;

    VkWriteDescriptorSet asWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    asWrite.dstSet = descriptorSet;
    asWrite.dstBinding = 0;
    asWrite.descriptorCount = 1;
    asWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    asWrite.pNext = &asInfo;

    VkDescriptorImageInfo outImageInfo{};
    outImageInfo.imageView = rtImageView;     // your storage image view
    outImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;     // IMPORTANT

    VkWriteDescriptorSet imgWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    imgWrite.dstSet = descriptorSet;
    imgWrite.dstBinding = 1;
    imgWrite.descriptorCount = 1;
    imgWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imgWrite.pImageInfo = &outImageInfo;

    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = cameraBuffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(Camera);

    VkWriteDescriptorSet cameraDescriptorWrite{};
    cameraDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraDescriptorWrite.dstSet = descriptorSet;
    cameraDescriptorWrite.dstBinding = 2;
    cameraDescriptorWrite.descriptorCount = 1;
    cameraDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraDescriptorWrite.pBufferInfo = &cameraBufferInfo;

    VkDescriptorBufferInfo vertexBufferInfo{};
    vertexBufferInfo.buffer = resources.vertexBuffer;
    vertexBufferInfo.offset = 0;
    vertexBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet vertexBufferWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    vertexBufferWrite.dstSet = descriptorSet;
    vertexBufferWrite.dstBinding = 3;
    vertexBufferWrite.descriptorCount = 1;
    vertexBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    vertexBufferWrite.pBufferInfo = &vertexBufferInfo;

    VkDescriptorBufferInfo indexBufferInfo{};
    indexBufferInfo.buffer = resources.indexBuffer;
    indexBufferInfo.offset = 0;
    indexBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet indexBufferWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    indexBufferWrite.dstSet = descriptorSet;
    indexBufferWrite.dstBinding = 4;
    indexBufferWrite.descriptorCount = 1;
    indexBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    indexBufferWrite.pBufferInfo = &indexBufferInfo;

    VkDescriptorBufferInfo materialBufferInfo{};
    materialBufferInfo.buffer = resources.materialBuffer;
    materialBufferInfo.offset = 0;
    materialBufferInfo.range = VK_WHOLE_SIZE;

    VkWriteDescriptorSet materialBufferWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    materialBufferWrite.dstSet = descriptorSet;
    materialBufferWrite.dstBinding = 5;
    materialBufferWrite.descriptorCount = 1;
    materialBufferWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialBufferWrite.pBufferInfo = &materialBufferInfo;

    VkDescriptorImageInfo accumImageInfo{};
    accumImageInfo.imageView = accumImageView;     // accumulation image view
    accumImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;     // IMPORTANT

    VkWriteDescriptorSet accumImgWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    accumImgWrite.dstSet = descriptorSet;
    accumImgWrite.dstBinding = 6;
    accumImgWrite.descriptorCount = 1;
    accumImgWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    accumImgWrite.pImageInfo = &accumImageInfo;

    std::vector<VkDescriptorImageInfo> textureImageInfos;

    for(auto& tex : textures)
    {
        VkDescriptorImageInfo texInfo{};
        texInfo.imageView = tex.imageView;
        texInfo.sampler = tex.sampler;
        texInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        textureImageInfos.push_back(texInfo);
    }

    std::vector<VkWriteDescriptorSet> writes = { asWrite, imgWrite, cameraDescriptorWrite, vertexBufferWrite, indexBufferWrite, materialBufferWrite, accumImgWrite };

    if(!textureImageInfos.empty())
    {
        VkWriteDescriptorSet textureWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        textureWrite.dstSet = descriptorSet;
        textureWrite.dstBinding = 7;
        textureWrite.descriptorCount = static_cast<uint32_t>(textureImageInfos.size() > 0 ? textureImageInfos.size() : 1); // Avoid zero descriptor count
        textureWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        textureWrite.pImageInfo = textureImageInfos.data();
        textureWrite.dstArrayElement = 0;

        writes.push_back(textureWrite);
    }

    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}

void destroyDescriptors(VkDevice device, DescriptorBundle& d)
{
    vkDestroyDescriptorPool(device, d.pool, nullptr);
    vkDestroyDescriptorSetLayout(device, d.layout, nullptr);
}
