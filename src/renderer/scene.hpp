#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <string>
#include <iostream>


struct alignas(16) GPUVertex {
    glm::vec4 position; //xyz
    glm::vec4 normal; //xyz
    glm::vec2 uv; //xy
    glm::vec2 pad;
    glm::uvec4 materialID; //x
};
static_assert(sizeof(GPUVertex) == 64);

struct GPUMaterial
{
    glm::vec4 diffuse; //Kd
    glm::vec4 specular; //Ks, Ns
    glm::vec4 emissive; //Ke
    glm::uvec4 textureInfo; //x = hasTexture, y = textureIndex
};
static_assert(sizeof(GPUMaterial) == 64);

struct Texture
{
    std::string diffuse;
    VkImage image;
    VkDeviceMemory memory;
    VkSampler sampler;
    VkImageView imageView;

    void destroy(VkDevice device)
    {
        vkDestroySampler(device, sampler, nullptr);
        vkDestroyImageView(device, imageView, nullptr);
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
};

struct MeshData
{
    std::vector<GPUVertex> vertices;
    std::vector<uint32_t> indices;
};

struct SceneData
{
    MeshData mesh;
    std::vector<GPUMaterial> materials;
    std::vector<Texture> textures;
};
