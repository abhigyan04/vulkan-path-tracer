#define TINYOBJLOADER_DISABLE_FAST_FLOAT
#define TINYOBJLOADER_IMPLEMENTATION

#include "objLoader.hpp"
#include "../include/tinyobjloader/tiny_obj_loader.h"
#include <iostream>


SceneData loadOBJ(const std::string& path)
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
        defaultMat.diffuse = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
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

                scene.mesh.vertices.push_back({pos, 0.0f, normal, 0.0f});
            }

            scene.mesh.indices.push_back(baseIndex + 0);
            scene.mesh.indices.push_back(baseIndex + 1);
            scene.mesh.indices.push_back(baseIndex + 2);

            scene.mesh.materialIDs.push_back(static_cast<uint32_t>(matID));

            indexOffset += fv;

            // tinyobj::index_t idx0 = shape.mesh.indices[indexOffset + 0];
            // tinyobj::index_t idx1 = shape.mesh.indices[indexOffset + 1];
            // tinyobj::index_t idx2 = shape.mesh.indices[indexOffset + 2];

            // glm::vec3 v0 = {
            //     attrib.vertices[3 * idx0.vertex_index + 0],
            //     attrib.vertices[3 * idx0.vertex_index + 1],
            //     attrib.vertices[3 * idx0.vertex_index + 2]
            // };

            // glm::vec3 v1 = {
            //     attrib.vertices[3 * idx1.vertex_index + 0],
            //     attrib.vertices[3 * idx1.vertex_index + 1],
            //     attrib.vertices[3 * idx1.vertex_index + 2]
            // };

            // glm::vec3 v2 = {
            //     attrib.vertices[3 * idx2.vertex_index + 0],
            //     attrib.vertices[3 * idx2.vertex_index + 1],
            //     attrib.vertices[3 * idx2.vertex_index + 2]
            // };

            // glm::vec3 faceNormal = glm::normalize(glm::cross(v1 - v0, v2 - v0));

            // normals[idx0.vertex_index] += faceNormal;
            // normals[idx1.vertex_index] += faceNormal;
            // normals[idx2.vertex_index] += faceNormal;

            // indexOffset += fv;
        }
    }

    std::cout<<"Loaded scene: \n";
    std::cout << "Vertices: " << scene.mesh.vertices.size() << "\n";
    std::cout << "Indices: " << scene.mesh.indices.size() << "\n";
    std::cout << "Materials: " << scene.materials.size() << "\n";

    return scene;

    // for(auto& n : normals) {
    //     n = glm::normalize(n);
    // }

    // for (const auto& shape : shapes) {
    //     size_t indexOffset = 0;
    //     for(size_t f = 0 ; f < shape.mesh.num_face_vertices.size() ; f ++) {
    //         int fv = shape.mesh.num_face_vertices[f];
    //         if(fv != 3) {
    //             indexOffset += fv;
    //             continue;
    //         }
    //         int matID = shape.mesh.material_ids[f];
    //         // std::cout << "Material ID: " << matID << std::endl;
    //         glm::vec3 color(0.8f);
    //         if(matID >= 0 && matID < materials.size()) {
    //             const auto& mat = materials[matID];
    //             color = glm::vec3(mat.diffuse[0], mat.diffuse[1], mat.diffuse[2]);
    //         }
    //         else
    //             matID = 0;
            
    //         for(int v = 0 ; v < 3 ; v++) {
    //             tinyobj::index_t idx = shape.mesh.indices[indexOffset + v];

    //             glm::vec3 pos = {
    //                 attrib.vertices[3 * idx.vertex_index + 0],
    //                 attrib.vertices[3 * idx.vertex_index + 1],
    //                 attrib.vertices[3 * idx.vertex_index + 2]
    //             };

    //             glm::vec3 normal = normals[idx.vertex_index];

    //             modelData.mesh.vertices.push_back({pos, 0.0f, normal, 0.0f, color, 0.0f, matID});
    //             modelData.mesh.indices.push_back(static_cast<uint32_t>(modelData.mesh.indices.size()));
    //         }

    //         indexOffset += fv;

    //         std::cout << "Face " << f << ", Material ID: " << matID << std::endl;
    //     }
    // }

    // std::cout<< "Loaded OBJ file: " << path << " with " << modelData.mesh.vertices.size() << " vertices and " << modelData.mesh.indices.size() << " indices." << std::endl;

    // std::cout << "Materials loaded: " << materials.size() << std::endl;

    // return modelData;
}
