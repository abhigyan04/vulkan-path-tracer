#pragma once

#include <vector>
#include "../../../../../VulkanSDK/1.3.290.0/Include/glm/ext/vector_float3.hpp"


struct Vertex {
    glm::vec3 position;
    float pad0;

    glm::vec3 normal;
    float pad1;

    glm::vec3 color;
    float pad2;

    int materialID;
    int pad3;
    int pad4;
    int pad5;
};

struct Material
{
    glm::vec3 diffuse;
    float pad0;

    glm::vec3 specular;
    float shininess;

    glm::vec3 emissive;
    float pad1;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};

struct ModelData
{
    Mesh mesh;
    std::vector<Material> materials;
};
