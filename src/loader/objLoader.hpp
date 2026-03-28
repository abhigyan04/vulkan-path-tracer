#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include "../renderer/scene.hpp"
#include "../renderer/gpuScene.hpp"


SceneData loadOBJ(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string&  path);
Texture createTextureImage(VkDevice device, VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue, const std::string& path);
