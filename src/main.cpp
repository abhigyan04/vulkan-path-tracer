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

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

float yaw   = 90.0f;
float pitch = 0.0f;

double lastX = 0.0;
double lastY = 0.0;

bool firstMouse = true;

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

//Read file helper function
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filename);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

//Create shader module helper function
VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create shader module!");
    }

    return shaderModule;
}

//Find memory type helper function
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw std::runtime_error("Failed to find suitable memory type!");
}

//Createt buffer helper function
void createBuffer(VkDevice device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{};
    allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    allocFlagsInfo.flags = 0;

    //Required for SBT and AS later
    if(usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties);

    if(allocFlagsInfo.flags != 0) {
        allocInfo.pNext = &allocFlagsInfo;
    }

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

//Get buffer device address helper function
VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo bufferDeviceAI{};
    bufferDeviceAI.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferDeviceAI.buffer = buffer;
    return vkGetBufferDeviceAddress(device, &bufferDeviceAI);
}

//Begin single time command buffer helper function
VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer VkCommandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &VkCommandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(VkCommandBuffer, &beginInfo);

    return VkCommandBuffer;
}

//End single time command buffer helper function
void endSingleTimeCommands(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

//Copy buffer helper function
void copyBuffer(
    VkDevice device,
    VkCommandPool commandPool,
    VkQueue graphicsQueue,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize size)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
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
    pitch += yOffset;

    if(pitch > 89.0f)
        pitch = 89.0f;
    
    if(pitch < -89.0f)
        pitch = -89.0f;
    
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

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
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

    //Load Ray Tracing function pointers
    auto vkCreateRayTracingPipelinesKHR =
        (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    auto vkGetRayTracingShaderGroupHandlesKHR =
        (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(device, "vkGetRayTracingShaderGroupHandlesKHR");
    auto vkCmdTraceRaysKHR =
        (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");
    auto vkCreateAccelerationStructureKHR =
        (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR");
    auto vkCmdBuildAccelerationStructuresKHR =
        (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR");
    auto vkGetAccelerationStructureBuildSizesKHR =
        (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR");
    auto vkDestroyAccelerationStructureKHR =
        (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
    auto vkGetAccelerationStructureDeviceAddressKHR =
        (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR");

    if(!vkCreateRayTracingPipelinesKHR || !vkGetRayTracingShaderGroupHandlesKHR || !vkCmdTraceRaysKHR || !vkCreateAccelerationStructureKHR || !vkCmdBuildAccelerationStructuresKHR || !vkGetAccelerationStructureBuildSizesKHR || !vkDestroyAccelerationStructureKHR || !vkGetAccelerationStructureDeviceAddressKHR) {
        std::cerr << "Failed to load ray tracing function pointers!" << std::endl;
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

    //Descriptor Set Layout for storage image
    // VkDescriptorSetLayout descriptorSetLayout;

    // VkDescriptorSetLayoutBinding storageImageBinding{};
    // storageImageBinding.binding = 0;
    // storageImageBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    // storageImageBinding.descriptorCount = 1;
    // storageImageBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    // VkDescriptorSetLayoutCreateInfo layoutInfo{};
    // layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    // layoutInfo.bindingCount = 1;
    // layoutInfo.pBindings = &storageImageBinding;

    // if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
    //     std::cerr << "Failed to create descriptor set layout!" << std::endl;
    //     return -1;
    // }

    //Descriptor Pool and Descriptor Set for storage image
    // VkDescriptorPool descriptorPool;
    // VkDescriptorSet descriptorSet;

    // VkDescriptorPoolSize poolSize{};
    // poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    // poolSize.descriptorCount = 1;

    // VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    // descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    // descriptorPoolInfo.maxSets = 1;
    // descriptorPoolInfo.poolSizeCount = 1;
    // descriptorPoolInfo.pPoolSizes = &poolSize;

    // if (vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
    //     std::cerr << "Failed to create descriptor pool!" << std::endl;
    //     return -1;
    // }

    // VkDescriptorSetAllocateInfo descriptorAllocInfo{};
    // descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    // descriptorAllocInfo.descriptorPool = descriptorPool;
    // descriptorAllocInfo.descriptorSetCount = 1;
    // descriptorAllocInfo.pSetLayouts = &descriptorSetLayout;

    // if (vkAllocateDescriptorSets(device, &descriptorAllocInfo, &descriptorSet) != VK_SUCCESS) {
    //     std::cerr << "Failed to allocate descriptor set!" << std::endl;
    //     return -1;
    // }

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

    //Update descriptor to point at rtImageView
    // VkDescriptorImageInfo descriptorImageInfo{};
    // descriptorImageInfo.imageView = rtImageView;
    // descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // VkWriteDescriptorSet descriptorWrite{};
    // descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    // descriptorWrite.dstSet = descriptorSet;
    // descriptorWrite.dstBinding = 0;
    // descriptorWrite.dstArrayElement = 0;
    // descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    // descriptorWrite.descriptorCount = 1;
    // descriptorWrite.pImageInfo = &descriptorImageInfo;

    // vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

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

    //Create Render Pass
    // VkRenderPass renderPass;

    // VkAttachmentDescription colorAttachment{};
    // colorAttachment.format = surfaceFormat.format;
    // colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    // colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    // colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    // colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    // colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    // colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // VkAttachmentReference colorAttachmentRef{};
    // colorAttachmentRef.attachment = 0;
    // colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // VkSubpassDescription subpass{};
    // subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    // subpass.colorAttachmentCount = 1;
    // subpass.pColorAttachments = &colorAttachmentRef;

    // VkSubpassDependency dependency{};
    // dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    // dependency.dstSubpass = 0;
    // dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // dependency.srcAccessMask = 0;
    // dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    // dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // VkRenderPassCreateInfo renderPassInfo{};
    // renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    // renderPassInfo.attachmentCount = 1;
    // renderPassInfo.pAttachments = &colorAttachment;
    // renderPassInfo.subpassCount = 1;
    // renderPassInfo.pSubpasses = &subpass;
    // renderPassInfo.dependencyCount = 1;
    // renderPassInfo.pDependencies = &dependency;

    // if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
    //     std::cerr << "Failed to create render pass!" << std::endl;
    //     return -1;
    // }

    // std::cout << "Render pass created successfully!" << std::endl;

    //Create Framebuffers
    // std::vector<VkFramebuffer> swapChainFramebuffers(swapChainImageViews.size());

    // for(size_t i = 0; i < swapChainImageViews.size(); i++) {
    //     VkImageView attachments[] = {swapChainImageViews[i]};

    //     VkFramebufferCreateInfo framebufferInfo{};
    //     framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    //     framebufferInfo.renderPass = renderPass;
    //     framebufferInfo.attachmentCount = 1;
    //     framebufferInfo.pAttachments = attachments;
    //     framebufferInfo.width = extent.width;
    //     framebufferInfo.height = extent.height;
    //     framebufferInfo.layers = 1;

    //     if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS) {
    //         std::cerr << "Failed to create framebuffer!" << std::endl;
    //         return -1;
    //     }
    // }

    // std::cout << "Framebuffers created successfully!" << std::endl;

    //Create command buffers

    std::vector<VkCommandBuffer> commandBuffers(swapChainImages.size());

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
    const int MAX_FRAMES_IN_FLIGHT = 2;

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

    uint32_t currentFrame = 0;

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

    //Triangle vertex buffer
    struct Vertex {
        float pos[3];
    };

    std::vector<Vertex> vertices = {
        {{-1.0f, -1.0f, 0.0f}},
        {{1.0f, -1.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}}
    };

    //Create staging buffer
    VkDeviceSize bufferSize = sizeof(Vertex) * vertices.size();
    
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        bufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer,
        stagingBufferMemory
    );

    //Copy vertex data to staging buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertices.data(), (size_t)bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    //Create GPU buffer
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        bufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        vertexBuffer,
        vertexBufferMemory);

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

    camera.position = glm::vec3(0,0,-3);
    camera.forward  = glm::vec3(0,0,1);
    camera.right    = glm::vec3(1,0,0);
    camera.up       = glm::vec3(0,1,0);
    camera.fov      = glm::radians(45.0);

    void* cameraData;
    vkMapMemory(device, cameraBufferMemory, 0, sizeof(Camera), 0, &cameraData);
    memcpy(cameraData, &camera, sizeof(Camera));
    vkUnmapMemory(device, cameraBufferMemory);

    //Copy data from staging buffer to GPU buffer
    copyBuffer(device, commandPool, graphicsQueue, stagingBuffer, vertexBuffer, bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);

    VkDeviceAddress vertexAddress = getBufferDeviceAddress(device, vertexBuffer);

    //Describe geometry
    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    geometry.geometry.triangles.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = vertexAddress;
    geometry.geometry.triangles.vertexStride = sizeof(Vertex);
    geometry.geometry.triangles.maxVertex = 3;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_NONE_KHR;
    geometry.geometry.triangles.transformData.deviceAddress = 0;

    //Query sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    uint32_t primitiveCount = 1; // One triangle

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );

    //Create BLAS buffer
    VkBuffer blasBuffer;
    VkDeviceMemory blasMemory;

    createBuffer(
        device,
        physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blasBuffer,
        blasMemory
    );

    VkAccelerationStructureKHR blas;
    VkAccelerationStructureCreateInfoKHR asCreateInfo{};
    asCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    asCreateInfo.buffer = blasBuffer;
    asCreateInfo.size = sizeInfo.accelerationStructureSize;
    asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    vkCreateAccelerationStructureKHR(device, &asCreateInfo, nullptr, &blas);

    //Create scratch buffer
    VkBuffer scratchBuffer;
    VkDeviceMemory scratchMemory;

    createBuffer(
        device,
        physicalDevice,
        sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        scratchBuffer,
        scratchMemory
    );

    VkDeviceAddress scratchAddress = 
        getBufferDeviceAddress(device, scratchBuffer);

    //Finalize build info
    buildInfo.dstAccelerationStructure = blas;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;

    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    //Record command buffer to build BLAS
    VkCommandBuffer buildCmdBuffer = beginSingleTimeCommands(device, commandPool);
    
    vkCmdBuildAccelerationStructuresKHR(
        buildCmdBuffer,
        1,
        &buildInfo,
        &pRangeInfo
    );

    // Barrier: build -> usable by ray tracing
    VkMemoryBarrier blasBarrier{};
    blasBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    blasBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    blasBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        buildCmdBuffer,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &blasBarrier,
        0, nullptr,
        0, nullptr
    );

    endSingleTimeCommands(device, commandPool, graphicsQueue, buildCmdBuffer);

    vkDestroyBuffer(device, scratchBuffer, nullptr);
    vkFreeMemory(device, scratchMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = blas;

    VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);
    std::cout << "BLAS address: " << blasAddr << "\n";

    //Create instance struct for TLAS
    VkAccelerationStructureInstanceKHR accelStructureInstance{};

    accelStructureInstance.transform = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0
    };

    accelStructureInstance.instanceCustomIndex = 0;
    accelStructureInstance.mask = 0xFF;
    accelStructureInstance.instanceShaderBindingTableRecordOffset = 0;
    accelStructureInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    accelStructureInstance.accelerationStructureReference = blasAddr;

    //Create staging buffer
    VkDeviceSize accelBufferSize = sizeof(accelStructureInstance) * 1; // One instance
    
    VkBuffer accelStagingBuffer;
    VkDeviceMemory accelStagingBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        accelBufferSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        accelStagingBuffer,
        accelStagingBufferMemory
    );

    //Copy instance data to staging buffer
    void* accelData;
    vkMapMemory(device, accelStagingBufferMemory, 0, accelBufferSize, 0, &accelData);
    memcpy(accelData, &accelStructureInstance, (size_t)accelBufferSize);
    vkUnmapMemory(device, accelStagingBufferMemory);

    //Create GPU buffer for TLAS instances
    VkBuffer accelBuffer;
    VkDeviceMemory accelBufferMemory;

    createBuffer(
        device,
        physicalDevice,
        accelBufferSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        accelBuffer,
        accelBufferMemory);

    //Copy data from staging buffer to GPU buffer
    copyBuffer(device, commandPool, graphicsQueue, accelStagingBuffer, accelBuffer, accelBufferSize);

    vkDestroyBuffer(device, accelStagingBuffer, nullptr);
    vkFreeMemory(device, accelStagingBufferMemory, nullptr);

    //Describe TLAS geometry
    VkDeviceAddress accelBufferAddress = getBufferDeviceAddress(device, accelBuffer);

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;

    tlasGeometry.geometry.instances.sType = 
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

    tlasGeometry.geometry.instances.arrayOfPointers = VK_FALSE;
    tlasGeometry.geometry.instances.data.deviceAddress = accelBufferAddress;
    tlasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    //Build TLAS
    //Query sizes
    VkAccelerationStructureBuildGeometryInfoKHR accelBuildInfo{};
    accelBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    accelBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    accelBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    accelBuildInfo.geometryCount = 1;
    accelBuildInfo.pGeometries = &tlasGeometry;
    accelBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;

    uint32_t accelPrimitiveCount = 1; // One instance

    VkAccelerationStructureBuildSizesInfoKHR accelSizeInfo{};
    accelSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelBuildInfo,
        &accelPrimitiveCount,
        &accelSizeInfo
    );

    //Create TLAS buffer
    VkBuffer tlasBuffer;
    VkDeviceMemory tlasMemory;

    createBuffer(
        device,
        physicalDevice,
        accelSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        tlasBuffer,
        tlasMemory
    );

    VkAccelerationStructureKHR tlas;
    VkAccelerationStructureCreateInfoKHR tlasAsCreateInfo{};
    tlasAsCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasAsCreateInfo.buffer = tlasBuffer;
    tlasAsCreateInfo.size = accelSizeInfo.accelerationStructureSize;
    tlasAsCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    vkCreateAccelerationStructureKHR(device, &tlasAsCreateInfo, nullptr, &tlas);

    //Create scratch buffer
    VkBuffer accelScratchBuffer;
    VkDeviceMemory accelScratchMemory;

    createBuffer(
        device,
        physicalDevice,
        accelSizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
        VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        accelScratchBuffer,
        accelScratchMemory
    );

    VkDeviceAddress accelScratchAddress = 
        getBufferDeviceAddress(device, accelScratchBuffer);

    //Finalize build info
    accelBuildInfo.dstAccelerationStructure = tlas;
    accelBuildInfo.scratchData.deviceAddress = accelScratchAddress;

    VkAccelerationStructureBuildRangeInfoKHR accelRangeInfo{};
    accelRangeInfo.primitiveCount = accelPrimitiveCount;

    const VkAccelerationStructureBuildRangeInfoKHR* accelRanges[] = { &accelRangeInfo };

    //Record command buffer to build TLAS
    VkCommandBuffer accelBuildCmdBuffer = beginSingleTimeCommands(device, commandPool);
    
    vkCmdBuildAccelerationStructuresKHR(
        accelBuildCmdBuffer,
        1,
        &accelBuildInfo,
        accelRanges
    );

    // Barrier: build -> usable by ray tracing
    VkMemoryBarrier accelBarrier{};
    accelBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    accelBarrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    accelBarrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;

    vkCmdPipelineBarrier(
        accelBuildCmdBuffer,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        1, &accelBarrier,
        0, nullptr,
        0, nullptr
    );

    endSingleTimeCommands(device, commandPool, graphicsQueue, accelBuildCmdBuffer);

    vkDestroyBuffer(device, accelScratchBuffer, nullptr);
    vkFreeMemory(device, accelScratchMemory, nullptr);

    VkAccelerationStructureDeviceAddressInfoKHR accelAddrInfo{};
    accelAddrInfo.sType =
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    accelAddrInfo.accelerationStructure = tlas;

    VkDeviceAddress tlasAddr =
        vkGetAccelerationStructureDeviceAddressKHR(device, &accelAddrInfo);

    std::cout << "TLAS address: " << tlasAddr << "\n";

    //Descriptor set for TLAS + output storage image
    VkDescriptorSetLayout tlasDescriptorSetLayout;

    VkDescriptorSetLayoutBinding asBinding{};
    asBinding.binding = 0;
    asBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    asBinding.descriptorCount = 1;
    asBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;

    VkDescriptorSetLayoutBinding imgBinding{};
    imgBinding.binding = 1;
    imgBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imgBinding.descriptorCount = 1;
    imgBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    VkDescriptorSetLayoutBinding cameraBufferBinding{};
    cameraBufferBinding.binding = 2;
    cameraBufferBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBufferBinding.descriptorCount = 1;
    cameraBufferBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;

    std::array<VkDescriptorSetLayoutBinding, 3> bindings = { asBinding, imgBinding, cameraBufferBinding };

    VkDescriptorSetLayoutCreateInfo tlasLayoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    tlasLayoutInfo.bindingCount = (uint32_t)bindings.size();
    tlasLayoutInfo.pBindings = bindings.data();

    vkCreateDescriptorSetLayout(device, &tlasLayoutInfo, nullptr, &tlasDescriptorSetLayout);

    //Descriptor Pool and Descriptor Set for storage image
    VkDescriptorPool tlasDescriptorPool;
    VkDescriptorSet tlasDescriptorSet;

    std::array<VkDescriptorPoolSize, 3> poolSizes{};
    poolSizes[0] = { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 };
    poolSizes[1] = { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 };
    poolSizes[2] = { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 };

    VkDescriptorPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &poolInfo, nullptr, &tlasDescriptorPool);

    VkDescriptorSetAllocateInfo tlasAllocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    tlasAllocInfo.descriptorPool = tlasDescriptorPool;
    tlasAllocInfo.descriptorSetCount = 1;
    tlasAllocInfo.pSetLayouts = &tlasDescriptorSetLayout;

    vkAllocateDescriptorSets(device, &tlasAllocInfo, &tlasDescriptorSet);

    VkWriteDescriptorSetAccelerationStructureKHR asInfo{
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR
    };
    asInfo.accelerationStructureCount = 1;
    asInfo.pAccelerationStructures = &tlas;

    //Create Pipeline Layout
    VkPipelineLayout pipelineLayout;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &tlasDescriptorSetLayout;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout!" << std::endl;
        return -1;
    }
    
    //Create Ray Tracing Pipeline
    auto raygenCode = readFile("shaders/raygen.spv");
    auto misscode = readFile("shaders/miss.spv");
    auto chitcode = readFile("shaders/chit.spv");
    VkShaderModule raygenShaderModule = createShaderModule(device, raygenCode);
    VkShaderModule missShaderModule = createShaderModule(device, misscode);
    VkShaderModule chitShaderModule = createShaderModule(device, chitcode);

    VkPipeline rayTracingPipeline;

    VkPipelineShaderStageCreateInfo shaderStages[3]{};

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    shaderStages[0].module = raygenShaderModule;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    shaderStages[1].module = missShaderModule;
    shaderStages[1].pName = "main";

    shaderStages[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[2].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    shaderStages[2].module = chitShaderModule;
    shaderStages[2].pName = "main";

    VkRayTracingShaderGroupCreateInfoKHR shaderGroups[3]{};

    //Group 0: raygen
    shaderGroups[0].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[0].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroups[0].generalShader = 0; // Index of raygen shader in shaderStages
    shaderGroups[0].closestHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[0].anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[0].intersectionShader = VK_SHADER_UNUSED_KHR;

    //Group 1: miss
    shaderGroups[1].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[1].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroups[1].generalShader = 1; // Index of miss shader in shaderStages
    shaderGroups[1].closestHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[1].anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[1].intersectionShader = VK_SHADER_UNUSED_KHR;

    //Group 2: hit
    shaderGroups[2].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[2].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    shaderGroups[2].generalShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[2].closestHitShader = 2; // Index of closest hit shader in shaderStages
    shaderGroups[2].anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[2].intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = (uint32_t)std::size(shaderStages);
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.groupCount = (uint32_t)std::size(shaderGroups);
    pipelineInfo.pGroups = shaderGroups;
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = pipelineLayout;

    if (vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rayTracingPipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create ray tracing pipeline!" << std::endl;
        return -1;
    }

    std::cout << "Ray tracing pipeline created successfully!" << std::endl;

    vkDestroyShaderModule(device, raygenShaderModule, nullptr);
    vkDestroyShaderModule(device, missShaderModule, nullptr);
    vkDestroyShaderModule(device, chitShaderModule, nullptr);

    //Build the SBT

    const uint32_t groupCount = pipelineInfo.groupCount;

    auto alignUp = [](VkDeviceSize v, VkDeviceSize a) {
        return (v + a - 1) & ~(a - 1);
    };

    VkDeviceSize handleSize   = rayTracingPipelineProperties.shaderGroupHandleSize;
    VkDeviceSize handleAlign  = rayTracingPipelineProperties.shaderGroupHandleAlignment;
    VkDeviceSize baseAlign    = rayTracingPipelineProperties.shaderGroupBaseAlignment;

    // Each record stride must be aligned to handle alignment
    VkDeviceSize recordStride = alignUp(handleSize, handleAlign);

    // Each region start must be aligned to base alignment
    VkDeviceSize raygenRegionSize = alignUp(recordStride, baseAlign);
    VkDeviceSize missRegionSize   = alignUp(recordStride, baseAlign);
    VkDeviceSize hitRegionSize    = alignUp(recordStride, baseAlign);

    VkDeviceSize sbtSize = raygenRegionSize + missRegionSize + hitRegionSize;

    //Alocate storage for shader group handles
    std::vector<uint8_t> handles(groupCount * handleSize);

    if (vkGetRayTracingShaderGroupHandlesKHR(
        device,
        rayTracingPipeline,
        0,
        groupCount,
        handles.size(),
        handles.data()) != VK_SUCCESS)
    {
        std::cerr << "Failed to get ray tracing shader group handles!" << std::endl;
        return -1;
    }

    //Create SBT buffer
    VkBuffer sbtBuffer;
    VkDeviceMemory sbtBufferMemory;

    createBuffer(device, physicalDevice, sbtSize,
    VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
    VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    sbtBuffer, sbtBufferMemory);

    //Wrtite shader group handles to SBT buffer
    uint8_t* mapped = nullptr;
    vkMapMemory(device, sbtBufferMemory, 0, sbtSize, 0, (void**)&mapped);

    // group 0 = raygen
    memcpy(mapped + 0,
       handles.data() + 0 * handleSize,
       handleSize);

    // group 1 = miss
    memcpy(mapped + raygenRegionSize,
       handles.data() + 1 * handleSize,
       handleSize);

    // group 2 = hit
    memcpy(mapped + raygenRegionSize + missRegionSize,
       handles.data() + 2 * handleSize,
       handleSize);

    vkUnmapMemory(device, sbtBufferMemory);

    //Create SBT buffer regions
    VkDeviceAddress sbtAddress = getBufferDeviceAddress(device, sbtBuffer);

    VkStridedDeviceAddressRegionKHR raygenSBT{};
    raygenSBT.deviceAddress = sbtAddress + 0;
    raygenSBT.stride = recordStride;
    raygenSBT.size   = recordStride;
    
    VkStridedDeviceAddressRegionKHR missSBT{};
    missSBT.deviceAddress = sbtAddress + raygenRegionSize;
    missSBT.stride = recordStride;
    missSBT.size   = recordStride;

    VkStridedDeviceAddressRegionKHR hitSBT{};
    hitSBT.deviceAddress = sbtAddress + raygenRegionSize + missRegionSize;
    hitSBT.stride = recordStride;
    hitSBT.size   = recordStride;

    VkStridedDeviceAddressRegionKHR callableSBT{};

    // const uint32_t groupCount = pipelineInfo.groupCount;

    // uint32_t groupHandleSize = rayTracingPipelineProperties.shaderGroupHandleSize;
    // uint32_t handleSizeAligned =
    //     (groupHandleSize + rayTracingPipelineProperties.shaderGroupHandleAlignment - 1) &
    //     ~(rayTracingPipelineProperties.shaderGroupHandleAlignment - 1);

    // VkDeviceSize sbtSize = groupCount * (VkDeviceSize)handleSizeAligned;

    // std::vector<uint8_t> handles(groupCount * groupHandleSize);

    // if (vkGetRayTracingShaderGroupHandlesKHR(
    //     device,
    //     rayTracingPipeline,
    //     0,
    //     groupCount,
    //     handles.size(),
    //     handles.data()) != VK_SUCCESS)
    // {
    //     std::cerr << "Failed to get ray tracing shader group handles!" << std::endl;
    //     return -1;
    // }

    // VkBuffer sbtBuffer;
    // VkDeviceMemory sbtBufferMemory;

    // createBuffer(device, physicalDevice, sbtSize,
    //     VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
    //     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    //     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
    //     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    //     sbtBuffer, sbtBufferMemory);

    // uint8_t* mapped = nullptr;
    // vkMapMemory(device, sbtBufferMemory, 0, sbtSize, 0, (void**)&mapped);

    // for (uint32_t g = 0 ; g < groupCount ; g++) {
    //     memcpy(mapped + g * handleSizeAligned, handles.data() + g * groupHandleSize, groupHandleSize);
    // }

    // vkUnmapMemory(device, sbtBufferMemory);

    // VkDeviceAddress sbtAddress = getBufferDeviceAddress(device, sbtBuffer);

    // //Create SBT buffer region
    // VkStridedDeviceAddressRegionKHR raygenSBT{};
    // raygenSBT.deviceAddress = sbtAddress + 0 * handleSizeAligned;
    // raygenSBT.stride = handleSizeAligned;
    // raygenSBT.size = handleSizeAligned;

    // VkStridedDeviceAddressRegionKHR missSBT{};
    // missSBT.deviceAddress = sbtAddress + 1 * handleSizeAligned;
    // missSBT.stride = handleSizeAligned;
    // missSBT.size = handleSizeAligned;

    // VkStridedDeviceAddressRegionKHR hitSBT{};
    // hitSBT.deviceAddress = sbtAddress + 2 * handleSizeAligned;
    // hitSBT.stride = handleSizeAligned;
    // hitSBT.size = handleSizeAligned;

    // VkStridedDeviceAddressRegionKHR callableSBT{};
    // callableSBT.deviceAddress = 0;
    // callableSBT.stride = 0;
    // callableSBT.size = 0;

    VkWriteDescriptorSet asWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    asWrite.dstSet = tlasDescriptorSet;
    asWrite.dstBinding = 0;
    asWrite.descriptorCount = 1;
    asWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    asWrite.pNext = &asInfo;

    VkDescriptorImageInfo outImageInfo{};
    outImageInfo.imageView = rtImageView;     // your storage image view
    outImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;     // IMPORTANT

    VkWriteDescriptorSet imgWrite{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    imgWrite.dstSet = tlasDescriptorSet;
    imgWrite.dstBinding = 1;
    imgWrite.descriptorCount = 1;
    imgWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    imgWrite.pImageInfo = &outImageInfo;

    VkDescriptorBufferInfo cameraBufferInfo{};
    cameraBufferInfo.buffer = cameraBuffer;
    cameraBufferInfo.offset = 0;
    cameraBufferInfo.range = sizeof(Camera);

     VkWriteDescriptorSet cameraDescriptorWrite{};
    cameraDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraDescriptorWrite.dstSet = tlasDescriptorSet;
    cameraDescriptorWrite.dstBinding = 2;
    cameraDescriptorWrite.descriptorCount = 1;
    cameraDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraDescriptorWrite.pBufferInfo = &cameraBufferInfo;

    std::array<VkWriteDescriptorSet, 3> writes = { asWrite, imgWrite, cameraDescriptorWrite };
    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);

    //Record Command Buffers
    auto recordCommandBuffer = [&](VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            std::cerr << "Failed to begin recording command buffer!" << std::endl;
            return;
        }

        //Bind + Trace
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline);
        vkCmdBindDescriptorSets(commandBuffer,
                                VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineLayout,
                                0,
                                1,
                                &tlasDescriptorSet,
                                0, nullptr);

        vkCmdTraceRaysKHR(commandBuffer,
            &raygenSBT,
            &missSBT,
            &hitSBT,
            &callableSBT,
            extent.width,
            extent.height,
            1);

        VkImageMemoryBarrier rtImageBarrier{};
        rtImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        rtImageBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        rtImageBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        rtImageBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        rtImageBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        rtImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        rtImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        rtImageBarrier.image = rtImage;
        rtImageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        rtImageBarrier.subresourceRange.baseMipLevel = 0;
        rtImageBarrier.subresourceRange.levelCount = 1;
        rtImageBarrier.subresourceRange.baseArrayLayer = 0;
        rtImageBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &rtImageBarrier
        );

        VkImageMemoryBarrier swapChainBarrier{};
        swapChainBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapChainBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapChainBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapChainBarrier.srcAccessMask = 0;
        swapChainBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        swapChainBarrier.image = swapChainImages[imageIndex];
        swapChainBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        swapChainBarrier.subresourceRange.baseMipLevel = 0;
        swapChainBarrier.subresourceRange.levelCount = 1;
        swapChainBarrier.subresourceRange.baseArrayLayer = 0;
        swapChainBarrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapChainBarrier
        );

        //Copy ray tracing output image to swap chain image
        VkImageCopy copyRegion{};
        copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.srcSubresource.layerCount = 1;
        copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.dstSubresource.layerCount = 1;
        copyRegion.extent.width = extent.width;
        copyRegion.extent.height = extent.height;
        copyRegion.extent.depth = 1;

        vkCmdCopyImage(
            commandBuffer,
            rtImage, VK_IMAGE_LAYOUT_GENERAL,
            swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &copyRegion
        );

        //Transition swap chain image to present layout
        swapChainBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapChainBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapChainBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        swapChainBarrier.dstAccessMask = 0;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &swapChainBarrier
        );

        vkEndCommandBuffer(commandBuffer);

        // VkClearValue clearColor = {};
        // clearColor.color = {{1.05f, 0.05f, 0.10f, 1.0f}};

        // VkRenderPassBeginInfo renderPassInfo{};
        // renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        // renderPassInfo.renderPass = renderPass;
        // renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        // renderPassInfo.renderArea.offset = {0, 0};
        // renderPassInfo.renderArea.extent = extent;

        // renderPassInfo.clearValueCount = 1;
        // renderPassInfo.pClearValues = &clearColor;

        // vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        // No drawing commands yet
        // vkCmdEndRenderPass(commandBuffer);

        // if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        //     std::cerr << "Failed to record command buffer!" << std::endl;
        //     return;
        // }
    };

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

        vkResetFences(device, 1, &inFlightFences[currentFrame]);
        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};

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

        glm::vec3 direction;

        direction.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        direction.y = sin(glm::radians(pitch));
        direction.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));

        camera.forward = glm::normalize(direction);
        camera.right = glm::normalize(glm::cross(camera.forward, glm::vec3(0, 1, 0)));
        camera.up = glm::normalize(glm::cross(camera.right, camera.forward));

        // std::cout << "Camera Position: " << camera.position.x << ", " << camera.position.y << ", " << camera.position.z << "\n";

        void* camData;
        vkMapMemory(device, cameraBufferMemory, 0, sizeof(Camera), 0, &camData);
        memcpy(camData, &camera, sizeof(Camera));
        vkUnmapMemory(device, cameraBufferMemory);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            std::cerr << "Failed to submit draw command buffer!" << std::endl;
        }

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
    if(tlasDescriptorPool) {
        vkDestroyDescriptorPool(device, tlasDescriptorPool, nullptr);
    }
    if(tlasDescriptorSetLayout) {
        vkDestroyDescriptorSetLayout(device, tlasDescriptorSetLayout, nullptr);
    }
    if(rayTracingPipeline) {
        vkDestroyPipeline(device, rayTracingPipeline, nullptr);
    }
    if(pipelineLayout) {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    }
    if(sbtBuffer) {
        vkDestroyBuffer(device, sbtBuffer, nullptr);
    }
    if(sbtBufferMemory) {
        vkFreeMemory(device, sbtBufferMemory, nullptr);
    }
    if(rtImageView) {
        vkDestroyImageView(device, rtImageView, nullptr);
    }
    if(rtImage) {
        vkDestroyImage(device, rtImage, nullptr);
    }
    if(rtImageMemory) {
        vkFreeMemory(device, rtImageMemory, nullptr);
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
    vkDestroyAccelerationStructureKHR(device, blas, nullptr);
    vkDestroyBuffer(device, blasBuffer, nullptr);
    vkFreeMemory(device, blasMemory, nullptr);
    vkDestroyBuffer(device, vertexBuffer, nullptr);
    vkFreeMemory(device, vertexBufferMemory, nullptr);
    vkDestroyAccelerationStructureKHR(device, tlas, nullptr);
    vkDestroyBuffer(device, tlasBuffer, nullptr);
    vkFreeMemory(device, tlasMemory, nullptr);
    vkDestroyBuffer(device, accelBuffer, nullptr);
    vkFreeMemory(device, accelBufferMemory, nullptr);
    vkDestroyBuffer(device, cameraBuffer, nullptr);
    vkFreeMemory(device, cameraBufferMemory, nullptr);
    // for(auto framebuffer : swapChainFramebuffers) {
    //     vkDestroyFramebuffer(device, framebuffer, nullptr);
    // }
    // vkDestroyRenderPass(device, renderPass, nullptr);
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
