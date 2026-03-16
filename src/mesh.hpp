#pragma once

#include <vector>
#include "../../../../../VulkanSDK/1.3.290.0/Include/glm/ext/vector_float3.hpp"


struct Vertex {
    glm::vec3 position;
    float pad1;

    glm::vec3 normal;
    float pad2;
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
};
