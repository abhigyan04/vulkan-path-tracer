#pragma once

#include <glm/glm.hpp>


struct Camera
{
    glm::vec3 position;
        float pad1;

        glm::vec3 forward;
        float pad2;

        glm::vec3 right;
        float pad3;

        glm::vec3 up;
        float pad4;

        float fov;
};
