#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION

#include "objLoader.hpp"
#include "../include/tinyobjloader/tiny_obj_loader.h"
#include <iostream>


Mesh loadOBJ(const std::string& path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
        std::cerr << "Failed to load OBJ file: " << err << std::endl;
        return {};
    }

    Mesh mesh;

    for (const auto& shape : shapes) {
        for (const auto& index : shape.mesh.indices) {
            Vertex vertex{};

            vertex.position = {
                static_cast<float>(attrib.vertices[3 * index.vertex_index + 0]),
                static_cast<float>(attrib.vertices[3 * index.vertex_index + 1]),
                static_cast<float>(attrib.vertices[3 * index.vertex_index + 2])
            };

            if (index.normal_index >= 0) {
                vertex.normal = {
                    static_cast<float>(attrib.normals[3 * index.normal_index + 0]),
                    static_cast<float>(attrib.normals[3 * index.normal_index + 1]),
                    static_cast<float>(attrib.normals[3 * index.normal_index + 2])
                };
            } else {
                vertex.normal = {0.0f, 0.0f, 0.0f};
            }

            mesh.vertices.push_back(vertex);
            mesh.indices.push_back(static_cast<uint32_t>(mesh.indices.size()));
        }
    }

    std::cout<< "Loaded OBJ file: " << path << " with " << mesh.vertices.size() << " vertices and " << mesh.indices.size() << " indices." << std::endl;

    return mesh;
}
