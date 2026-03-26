#pragma once

#include <vector>
#include <glm/glm.hpp>


struct GPUVertex {
    glm::vec3 position;
    float pad0;

    glm::vec3 normal;
    float pad1;
};

struct GPUMaterial
{
    glm::vec4 diffuse; //Kd
    glm::vec4 specular; //Ks, Ns
    glm::vec4 emissive; //Ke
};

struct MeshData
{
    std::vector<GPUVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> materialIDs;
};

struct SceneData
{
    MeshData mesh;
    std::vector<GPUMaterial> materials;
};
