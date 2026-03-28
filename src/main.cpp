#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <optional>
#include <cstring>
#include <algorithm>
#include <array>
#include <fstream>
#include <glm/ext/vector_float3.hpp>
#include <glm/trigonometric.hpp>
#include <glm/ext/quaternion_geometric.hpp>

#include "tiny_obj_loader.h"
#include "renderer/scene.hpp"
#include "renderer/gpuScene.hpp"
#include "renderer/acceleration.hpp"
#include "renderer/pipeline.hpp"
#include "renderer/commands.hpp"
#include "renderer/sbt.hpp"
#include "renderer/descriptors.hpp"
#include "loader/objLoader.hpp"


const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

float yaw   = 90.0f;
float pitch = 0.0f;

double lastX = 0.0;
double lastY = 0.0;

bool firstMouse = true;

const int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
    VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
    VK_KHR_SPIRV_1_4_EXTENSION_NAME,
    VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    for(const char* required: deviceExtensions) {
        bool found = false;
        for(const auto& ext : availableExtensions) {
            if (strcmp(required, ext.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; //always available
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        VkExtent2D actualExtent = {WIDTH, HEIGHT};

        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

//Implement camera rotation based on mouse movement
void mouseCallback(GLFWwindow* window, double xpos, double ypos)
{
    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS)
    {
        firstMouse = true;
        return;
    }

    float sensitivity = 0.1f;

    if(firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xOffset = xpos - lastX;
    float yOffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    xOffset *= sensitivity;
    yOffset *= sensitivity;

    yaw += xOffset;
    pitch -= yOffset;

    if(pitch > 89.0f)
        pitch = 89.0f;
    
    if(pitch < -89.0f)
        pitch = -89.0f;
    
    pitch = glm::clamp(pitch, -89.0f, 89.0f);
}

int main(){
    try {
    //Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan RT", nullptr, nullptr);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

    glfwSetCursorPosCallback(window, mouseCallback);

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vulkan RT";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    // appInfo.pEngineName = "No Engine";
    // appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();

    VkInstance instance;
    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        std::cerr << "Failed to create Vulkan instance!" << std::endl;
        return -1;
    }

    //Create Surface
    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface!" << std::endl;
        return -1;
    }

    std::cout << "Vulkan instance and surface created successfully!" << std::endl;

    // Enumerate Physical Devices
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    for(const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        std::cout << "Found GPU: " << props.deviceName << std::endl;
    }

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    for(const auto& device : devices) {
        QueueFamilyIndices indices = findQueueFamilies(device, surface);

        if(indices.isComplete() && checkDeviceExtensionSupport(device)) {
            physicalDevice = device;
            break;
        }
    }

    if(physicalDevice == VK_NULL_HANDLE) {
        std::cerr << "Failed to find a suitable GPU!" << std::endl;
        return -1;
    }

    std::cout << "NVIDIA RTX GPU selected!" << std::endl;

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

    float queuePriority = 1.0f;

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    //Enable ray tracing features
    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;
    rayTracingPipelineFeatures.pNext = &bufferDeviceAddressFeatures;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructureFeatures.accelerationStructure = VK_TRUE;
    accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

    // VkPhysicalDeviceVulkan12Features vulkan12Features{};
    // vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_12_FEATURES;
    // vulkan12Features.runtimeDescriptorArray = VK_TRUE;
    // vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
    // vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    // deviceCreateInfo.pNext = &vulkan12Features;
    deviceCreateInfo.pNext = &accelerationStructureFeatures;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());

    VkDevice device;
    if (vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device) != VK_SUCCESS) {
        std::cerr << "Failed to create logical device!" << std::endl;
        return -1;
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rayTracingPipelineProperties{};
    rayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 deviceProperties2{};
    deviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    deviceProperties2.pNext = &rayTracingPipelineProperties;

    vkGetPhysicalDeviceProperties2(physicalDevice, &deviceProperties2);

    VkQueue graphicsQueue;
    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);

    std::cout << "Logical device and graphics queue created successfully!" << std::endl;

    //Create common command pool
    VkCommandPool commandPool;

    VkCommandPoolCreateInfo commandPoolInfo{};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    commandPoolInfo.queueFamilyIndex = indices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &commandPoolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool!" << std::endl;
        return -1;
    }

    std::cout << "Command pool created successfully!" << std::endl;

    //Create Swap Chain
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapChainCreateInfo{};
    swapChainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapChainCreateInfo.surface = surface;
    swapChainCreateInfo.minImageCount = imageCount;
    swapChainCreateInfo.imageFormat = surfaceFormat.format;
    swapChainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapChainCreateInfo.imageExtent = extent;
    swapChainCreateInfo.imageArrayLayers = 1;
    swapChainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapChainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapChainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    swapChainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapChainCreateInfo.presentMode = presentMode;
    swapChainCreateInfo.clipped = VK_TRUE;
    swapChainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
    
    VkSwapchainKHR swapChain;
    if (vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapChain) != VK_SUCCESS) {
        std::cerr << "Failed to create swap chain!" << std::endl;
        return -1;
    }

    std::cout << "Swap chain created successfully!" << std::endl;

    //Create Ray Tracing Output Image
    VkImage rtImage;
    VkDeviceMemory rtImageMemory;
    VkImageView rtImageView;
    VkImage accumImage;
    VkDeviceMemory accumImageMemory;
    VkImageView accumImageView;

    VkImageCreateInfo rtImageCreateInfo{};
    rtImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    rtImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    rtImageCreateInfo.extent.width = extent.width;
    rtImageCreateInfo.extent.height = extent.height;
    rtImageCreateInfo.extent.depth = 1;
    rtImageCreateInfo.mipLevels = 1;
    rtImageCreateInfo.arrayLayers = 1;
    rtImageCreateInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    rtImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    rtImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    rtImageCreateInfo.usage =
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    rtImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    rtImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &rtImageCreateInfo, nullptr, &rtImage) != VK_SUCCESS) {
        std::cerr << "Failed to create ray tracing output image!" << std::endl;
        return -1;
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, rtImage, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{};
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo memoryAllocInfo{};
    memoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocInfo.allocationSize = memRequirements.size;
    memoryAllocInfo.memoryTypeIndex = 0; // Placeholder, should find proper memory type index
    memoryAllocInfo.pNext = &allocFlagsInfo;

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for(uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            memoryAllocInfo.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(device, &memoryAllocInfo, nullptr, &rtImageMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory for ray tracing output image!" << std::endl;
        return -1;
    }

    vkBindImageMemory(device, rtImage, rtImageMemory, 0);

    //Create Image Views for Swap Chain Images
    VkImageViewCreateInfo rtImageViewCreateInfo{};
    rtImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    rtImageViewCreateInfo.image = rtImage;
    rtImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    rtImageViewCreateInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
    rtImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    rtImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    rtImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    rtImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    rtImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    rtImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    rtImageViewCreateInfo.subresourceRange.levelCount = 1;
    rtImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    rtImageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &rtImageViewCreateInfo, nullptr, &rtImageView) != VK_SUCCESS) {
        std::cerr << "Failed to create image view for ray tracing output image!" << std::endl;
        return -1;
    }

    std::cout << "Ray tracing output image and image view created successfully!" << std::endl;

    //Allocate a temporary command pool and buffer for layout transition
    VkCommandBufferAllocateInfo cmdBufAllocInfo{};
    cmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdBufAllocInfo.commandPool = commandPool;
    cmdBufAllocInfo.commandBufferCount = 1;

    VkCommandBuffer tempCmdBuffer;
    if (vkAllocateCommandBuffers(device, &cmdBufAllocInfo, &tempCmdBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate temporary command buffer!" << std::endl;
        return -1;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(tempCmdBuffer, &beginInfo);

    //Transition ray tracing output image to general layout for shader access
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rtImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        tempCmdBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    vkEndCommandBuffer(tempCmdBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &tempCmdBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &tempCmdBuffer);

    VkImageCreateInfo accumImageCreateInfo{};
    accumImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    accumImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    accumImageCreateInfo.extent.width = extent.width;
    accumImageCreateInfo.extent.height = extent.height;
    accumImageCreateInfo.extent.depth = 1;
    accumImageCreateInfo.mipLevels = 1;
    accumImageCreateInfo.arrayLayers = 1;
    accumImageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    accumImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    accumImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    accumImageCreateInfo.usage =
        VK_IMAGE_USAGE_STORAGE_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    accumImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    accumImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &accumImageCreateInfo, nullptr, &accumImage) != VK_SUCCESS) {
        std::cerr << "Failed to create accumulation image!" << std::endl;
        return -1;
    }

    VkMemoryRequirements accumMemRequirements;
    vkGetImageMemoryRequirements(device, accumImage, &accumMemRequirements);

    VkMemoryAllocateFlagsInfo accumAllocFlagsInfo{};
    accumAllocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    accumAllocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo accumMemoryAllocInfo{};
    accumMemoryAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    accumMemoryAllocInfo.allocationSize = accumMemRequirements.size;
    accumMemoryAllocInfo.memoryTypeIndex = 0; // Placeholder, should find proper memory type index
    accumMemoryAllocInfo.pNext = &accumAllocFlagsInfo;

    VkPhysicalDeviceMemoryProperties accumMemProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &accumMemProperties);

    for(uint32_t i = 0; i < accumMemProperties.memoryTypeCount; i++) {
        if ((accumMemRequirements.memoryTypeBits & (1 << i)) &&
            (accumMemProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) {
            accumMemoryAllocInfo.memoryTypeIndex = i;
            break;
        }
    }

    if (vkAllocateMemory(device, &accumMemoryAllocInfo, nullptr, &accumImageMemory) != VK_SUCCESS) {
        std::cerr << "Failed to allocate memory for accumulation image!" << std::endl;
        return -1;
    }

    vkBindImageMemory(device, accumImage, accumImageMemory, 0);

    //Create Image Views for accumulation image
    VkImageViewCreateInfo accumImageViewCreateInfo{};
    accumImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    accumImageViewCreateInfo.image = accumImage;
    accumImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    accumImageViewCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    accumImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    accumImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    accumImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    accumImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    accumImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    accumImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    accumImageViewCreateInfo.subresourceRange.levelCount = 1;
    accumImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    accumImageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &accumImageViewCreateInfo, nullptr, &accumImageView) != VK_SUCCESS) {
        std::cerr << "Failed to create image view for accumulation image!" << std::endl;
        return -1;
    }

    std::cout << "Accumulation image and image view created successfully!" << std::endl;

    //Allocate a temporary command pool and buffer for accumulation image layout transition
    VkCommandBufferAllocateInfo accumCmdBufAllocInfo{};
    accumCmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    accumCmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    accumCmdBufAllocInfo.commandPool = commandPool;
    accumCmdBufAllocInfo.commandBufferCount = 1;

    VkCommandBuffer accumCmdBuffer;
    if (vkAllocateCommandBuffers(device, &accumCmdBufAllocInfo, &accumCmdBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate temporary command buffer!" << std::endl;
        return -1;
    }

    VkCommandBufferBeginInfo accumBeginInfo{};
    accumBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    accumBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(accumCmdBuffer, &accumBeginInfo);

    VkImageMemoryBarrier accumBarrier{};
    accumBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    accumBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    accumBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    accumBarrier.srcAccessMask = 0;
    accumBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    accumBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    accumBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    accumBarrier.image = accumImage;
    accumBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    accumBarrier.subresourceRange.baseMipLevel = 0;
    accumBarrier.subresourceRange.levelCount = 1;
    accumBarrier.subresourceRange.baseArrayLayer = 0;
    accumBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        accumCmdBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        0, nullptr,
        0, nullptr,
        1, &accumBarrier
    );

    vkEndCommandBuffer(accumCmdBuffer);

    VkSubmitInfo accumSubmitInfo{};
    accumSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    accumSubmitInfo.commandBufferCount = 1;
    accumSubmitInfo.pCommandBuffers = &accumCmdBuffer;

    vkQueueSubmit(graphicsQueue, 1, &accumSubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &accumCmdBuffer);

    //Create Image Views for Swap Chain Images
    uint32_t swapChainImageCount;
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, nullptr);
    std::vector<VkImage> swapChainImages(swapChainImageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &swapChainImageCount, swapChainImages.data());

    std::vector<VkImageView> swapChainImageViews(swapChainImageCount);

    for(size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = surfaceFormat.format;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create image views!" << std::endl;
            return -1;
        }
    }

    std::cout << "Image views created successfully!" << std::endl;

    //Transition swap chain images to present source layout
    VkCommandBufferAllocateInfo swapCmdBufAllocInfo{};
    swapCmdBufAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    swapCmdBufAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    swapCmdBufAllocInfo.commandPool = commandPool;
    swapCmdBufAllocInfo.commandBufferCount = 1;

    VkCommandBuffer tempSwapCmdBuffer;
    if (vkAllocateCommandBuffers(device, &swapCmdBufAllocInfo, &tempSwapCmdBuffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate temporary command buffer!" << std::endl;
        return -1;
    }

    VkCommandBufferBeginInfo swapBeginInfo{};
    swapBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    swapBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(tempSwapCmdBuffer, &swapBeginInfo);
    for(size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = 0;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = swapChainImages[i];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            tempSwapCmdBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

    vkEndCommandBuffer(tempSwapCmdBuffer);


    VkSubmitInfo tempSwapSubmitInfo{};
    tempSwapSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    tempSwapSubmitInfo.commandBufferCount = 1;
    tempSwapSubmitInfo.pCommandBuffers = &tempSwapCmdBuffer;

    vkQueueSubmit(graphicsQueue, 1, &tempSwapSubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &tempSwapCmdBuffer);

    //Create command buffers

    std::vector<VkCommandBuffer> commandBuffers(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo commandBufferAllocInfo{};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.commandPool = commandPool;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if(vkAllocateCommandBuffers(device, &commandBufferAllocInfo, commandBuffers.data()) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers!" << std::endl;
        return -1;
    }

    std::cout << "Command buffers created successfully!" << std::endl;

    //Create Synchronization Objects

    std::vector<VkSemaphore> imageAvailableSemaphores(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkSemaphore> renderFinishedSemaphores(MAX_FRAMES_IN_FLIGHT);
    std::vector<VkFence> inFlightFences(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create synchronization objects for a frame!" << std::endl;
            return -1;
        }
    }

    std::cout << "Synchronization objects created successfully!" << std::endl;

    //Camera
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

    //Camera setup
    VkBuffer cameraBuffer;
    VkDeviceMemory cameraBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        sizeof(Camera),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        cameraBuffer,
        cameraBufferMemory
    );

    Camera camera;

    camera.position = glm::vec3(0,1,2.5);
    camera.forward  = glm::normalize(glm::vec3(0, 1, 0) - camera.position);
    camera.right    = glm::normalize(glm::cross(camera.forward, glm::vec3(0,1,0)));
    camera.up       = glm::normalize(glm::cross(camera.right, camera.forward));
    camera.fov      = glm::radians(45.0);

    void* cameraData;
    vkMapMemory(device, cameraBufferMemory, 0, sizeof(Camera), 0, &cameraData);
    memcpy(cameraData, &camera, sizeof(Camera));
    vkUnmapMemory(device, cameraBufferMemory);

    // Load model
    SceneData scene = loadOBJ(device, physicalDevice, commandPool, graphicsQueue, "assets/CornellBox-Original.obj");
    // SceneData scene = loadOBJ(device, physicalDevice, commandPool, graphicsQueue, "assets/bunny.obj");
    // SceneData scene = loadOBJ(device, physicalDevice, commandPool, graphicsQueue, "assets/floatplane.obj");

    SceneGPUResources gpuScene = uploadSceneToGPU(device, physicalDevice, scene);

    AccelerationStructures accel = buildAccelerationStructures(device, physicalDevice, commandPool, graphicsQueue, scene, gpuScene);
    
    DescriptorBundle descriptors = createSceneDescriptorSet(device, accel.tlas, cameraBuffer, gpuScene, scene.textures);

    updateSceneDescriptors(device, descriptors.set, gpuScene, accel.tlas, rtImageView, accumImageView, cameraBuffer, scene.textures);

    RTPipeline pipeline = createRayTracingPipeline(device, descriptors.layout);

    SBT sbt = createSBT(device, physicalDevice, pipeline.pipeline);

    std::vector<VkFence> imagesInFlight(swapChainImages.size(), VK_NULL_HANDLE);

    uint32_t currentFrame = 0;

    uint32_t frameIndex = 0;

    //Main Loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

        uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(
            device, swapChain, UINT64_MAX,
            imageAvailableSemaphores[currentFrame],
            VK_NULL_HANDLE, &imageIndex
        );

        if(result != VK_SUCCESS) {
            std::cerr << "Failed to acquire swap chain image!" << std::endl;
            continue;
        }

        if(imagesInFlight[imageIndex] != VK_NULL_HANDLE) {
            vkWaitForFences(device, 1, &imagesInFlight[imageIndex], VK_TRUE, UINT64_MAX);
        }

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        bool cameraMoved = false;

        glm::vec3 oldPos = camera.position;
        glm::vec3 oldForward = camera.forward;

        float speed = 0.005f;

        if(glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.position += camera.forward * speed;
        if(glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.position -= camera.forward * speed;
        if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.position -= camera.right * speed;
        if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.position += camera.right * speed;
        if(glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
            camera.position -= camera.up * speed;
        if(glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
            camera.position += camera.up * speed;
        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, true);

        glm::vec3 direction;

        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        camera.forward = glm::normalize(-direction);
        camera.right = glm::normalize(glm::cross(camera.forward, glm::vec3(0, 1, 0)));
        camera.up = glm::normalize(glm::cross(camera.right, camera.forward));

        // std::cout << "Camera Position: " << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << "\n";

        void* camData;
        vkMapMemory(device, cameraBufferMemory, 0, sizeof(Camera), 0, &camData);
        memcpy(camData, &camera, sizeof(Camera));
        vkUnmapMemory(device, cameraBufferMemory);

        if(oldPos != camera.position || oldForward != camera.forward) {
            frameIndex = 0;
            cameraMoved = true;
        }
        else
        {
            frameIndex++;
        }

        recordCommandBuffer(device, commandBuffers[currentFrame], frameIndex, imageIndex, pipeline.pipeline, pipeline.layout, descriptors.set, rtImage, accumImage, swapChainImages, sbt, extent, cameraMoved);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VkResult submitResult = vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]);

        if (submitResult != VK_SUCCESS) {
            std::cerr << "vkQueueSubmit failed: " << submitResult << std::endl;
            abort();
        }

        imagesInFlight[imageIndex] = inFlightFences[currentFrame];

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain;
        presentInfo.pImageIndices = &imageIndex;

        VkResult presentResult = vkQueuePresentKHR(graphicsQueue, &presentInfo);
        if (presentResult != VK_SUCCESS) {
            std::cerr << "Failed to present swap chain image!" << std::endl;
        }

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(device);

    //Cleanup
    if(device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }
    destroySBT(device, sbt);
    destroyPipeline(device, pipeline);
    destroyDescriptors(device, descriptors);
    destroyAccelerationStructures(device, accel);
    destroySceneGPU(device, gpuScene, scene.textures);

    if(rtImageView) {
        vkDestroyImageView(device, rtImageView, nullptr);
    }
    if(rtImage) {
        vkDestroyImage(device, rtImage, nullptr);
    }
    if(rtImageMemory) {
        vkFreeMemory(device, rtImageMemory, nullptr);
    }
    if(accumImageView) {
        vkDestroyImageView(device, accumImageView, nullptr);
    }
    if(accumImage) {
        vkDestroyImage(device, accumImage, nullptr);
    }
    if(accumImageMemory) {
        vkFreeMemory(device, accumImageMemory, nullptr);
    }
    for(auto imageView : swapChainImageViews) {
        if(imageView) {
            vkDestroyImageView(device, imageView, nullptr);
        }
    }
    if(swapChain) {
        vkDestroySwapchainKHR(device, swapChain, nullptr);
    }
    for(int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if(imageAvailableSemaphores[i]) {
            vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        }
        if(renderFinishedSemaphores[i]) {
            vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        }
        if(inFlightFences[i]) {
            vkDestroyFence(device, inFlightFences[i], nullptr);
        }
    }
    if(commandPool) {
        vkDestroyCommandPool(device, commandPool, nullptr);
    }
    vkDestroyBuffer(device, cameraBuffer, nullptr);
    vkFreeMemory(device, cameraBufferMemory, nullptr);
    if(device) {
        vkDestroyDevice(device, nullptr);
    }
    if(surface) {
        vkDestroySurfaceKHR(instance, surface, nullptr);
    }
    if(instance) {
        vkDestroyInstance(instance, nullptr);
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
    }
    catch(const std::exception& e) {
        std::cerr << "An error occurred: " << e.what() << std::endl;
        return -1;
    }
}
