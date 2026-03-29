#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#include "objLoader.hpp"
#include "../include/tiny_obj_loader.h"
#include <iostream>
#include <vulkan/vulkan.h>
#include "../include/stb_image.h"


SceneData loadOBJ(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string& path)
{
    SceneData scene;

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    std::string baseDir = "assets/";

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str(), baseDir.c_str(), true)) {
        std::cerr << "Failed to load OBJ file: " << err << std::endl;
        return {};
    }

    //Materials
    for(const auto& mat : materials) {
        GPUMaterial m{};
        m.diffuse = glm::vec4(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2], 1.0f);
        m.specular = glm::vec4(mat.specular[0], mat.specular[1], mat.specular[2], mat.shininess);
        m.emissive = glm::vec4(mat.emission[0], mat.emission[1], mat.emission[2], 1.0f);

        scene.materials.push_back(m);
    }

    if(scene.materials.empty()) {
        std::cout<<"No materials found in OBJ file. Adding default material." << std::endl;
        GPUMaterial defaultMat{};
        defaultMat.diffuse = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f);
        scene.materials.push_back(defaultMat);
    }

    //Geometry
    std::vector<glm::vec3> normals(attrib.vertices.size() / 3, glm::vec3(0.0f));

    for (const auto& shape : shapes) {
        size_t indexOffset = 0;

        for(size_t f = 0 ; f < shape.mesh.num_face_vertices.size() ; f++) {

            int fv = shape.mesh.num_face_vertices[f];
            if(fv != 3) {
                std::cerr << "Non-triangular face detected. Skipping." << std::endl;
                indexOffset += fv;
                continue;
            }

            int matID = 0;
            if(f < shape.mesh.material_ids.size() && shape.mesh.material_ids[f] >= 0)
            {
                matID = shape.mesh.material_ids[f];
            }

            uint32_t baseIndex = static_cast<uint32_t>(scene.mesh.vertices.size());

            for(int v = 0 ; v < 3 ; v++) {
                tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

                glm::vec3 pos = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2]
                };

                glm::vec3 normal{0.0, 1.0, 0.0};

                if(idx.normal_index >= 0) {
                    normal = glm::normalize(glm::vec3{
                        attrib.normals[3 * idx.normal_index + 0],
                        attrib.normals[3 * idx.normal_index + 1],
                        attrib.normals[3 * idx.normal_index + 2]
                    });
                }

                glm::vec2 uv{0.0f, 0.0f};

                if(!attrib.texcoords.empty())
                {
                    uv = {
                        attrib.texcoords[2 * idx.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * idx.texcoord_index + 1]
                    };
                }

                scene.mesh.vertices.push_back({
                    glm::vec4(pos, 1.0f),
                    glm::vec4(normal, 0.0f),
                    uv,
                    glm::vec2(0.0f, 0.0f),
                    glm::uvec4(static_cast<uint32_t>(matID), 0u, 0u, 0u)
                });
            }

            scene.mesh.indices.push_back(baseIndex + 0);
            scene.mesh.indices.push_back(baseIndex + 1);
            scene.mesh.indices.push_back(baseIndex + 2);

            indexOffset += fv;
        }
    }

    //Textures
    for(size_t i = 0 ; i < materials.size() ; i++)
    {
        if(!materials[i].diffuse_texname.empty())
        {
            std::string path = baseDir + materials[i].diffuse_texname;
            Texture tex = createTextureImage(device, physicalDevice, commandPool, graphicsQueue, path);
            scene.textures.push_back(tex);

            scene.materials[i].textureInfo.x = 1;
            scene.materials[i].textureInfo.y = scene.textures.size() - 1;
        }
        else
        {
            scene.materials[i].textureInfo.x = 0;
            scene.materials[i].textureInfo.y = 0;
        }
        std::cout<<"Material "<<i<<": hasTexture = "<<scene.materials[i].textureInfo.x<<" textureIndex = "<<scene.materials[i].textureInfo.y<<"\n";
    }

    std::cout<<"Loaded scene: \n";
    std::cout << "Vertices: " << scene.mesh.vertices.size() << "\n";
    std::cout << "Indices: " << scene.mesh.indices.size() << "\n";
    std::cout << "Materials: " << scene.materials.size() << "\n";

    return scene;
}

Texture createTextureImage(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string& path)
{
    Texture tex;

    int width, height, channels;
    stbi_uc* pixels = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);

    if(!pixels) {
        std::cerr << "Failed to load texture image: " << path << std::endl;
        return {};
    }

    VkDeviceSize imageSize = width * height * 4;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(device, physicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);

    stbi_image_free(pixels);

    createImage(device, physicalDevice, width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex.image, tex.memory);

    VkCommandBuffer commandBuffer = beginSingleTimeCommands(device, commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = tex.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.baseArrayLayer = 0;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = { (uint32_t)width, (uint32_t)height, 1 };

    vkCmdCopyBufferToImage(
        commandBuffer,
        stagingBuffer,
        tex.image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(device, commandPool, graphicsQueue, commandBuffer);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = tex.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.baseArrayLayer = 0;

    vkCreateImageView(device, &viewInfo, nullptr, &tex.imageView);

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    vkCreateSampler(device, &samplerInfo, nullptr, &tex.sampler);

    tex.diffuse = path;
    tex.image = tex.image;
    tex.memory = tex.memory;
    tex.imageView = tex.imageView;
    tex.sampler = tex.sampler;
    return tex;
}
