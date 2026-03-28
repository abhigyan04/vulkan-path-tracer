#pragma once

#include <vector>
#include <glm/glm.hpp>


struct alignas(16) GPUVertex {
    glm::vec4 position; //xyz
    glm::vec4 normal; //xyz
    glm::uvec4 materialID; //x
};
static_assert(sizeof(GPUVertex) == 48);

struct GPUMaterial
{
    glm::vec4 diffuse; //Kd
    glm::vec4 specular; //Ks, Ns
    glm::vec4 emissive; //Ke
};
static_assert(sizeof(GPUMaterial) == 48);

struct MeshData
{
    std::vector<GPUVertex> vertices;
    std::vector<uint32_t> indices;
};

struct SceneData
{
    MeshData mesh;
    std::vector<GPUMaterial> materials;
};
