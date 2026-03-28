#include "pipeline.hpp"
#include <iostream>
#include <fstream>


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

RTPipeline createRayTracingPipeline(VkDevice device, VkDescriptorSetLayout descriptorSetLayout)
{
    auto vkCreateRayTracingPipelinesKHR =
        (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR");
    
    RTPipeline rtPipeline;

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    //Create Pipeline Layout
    VkPipelineLayout pipelineLayout;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        std::cerr << "Failed to create pipeline layout!" << std::endl;
        RTPipeline emptyPipeline{};
        return emptyPipeline;
    }

    //Create Ray Tracing Pipeline
    auto raygenCode = readFile("src/shaders/raygen.spv");
    auto misscode = readFile("src/shaders/miss.spv");
    auto chitcode = readFile("src/shaders/chit.spv");
    auto shadowmisscode = readFile("src/shaders/shadowmiss.spv");
    auto shadowhitcode = readFile("src/shaders/shadowhit.spv");
    VkShaderModule raygenShaderModule = createShaderModule(device, raygenCode);
    VkShaderModule missShaderModule = createShaderModule(device, misscode);
    VkShaderModule chitShaderModule = createShaderModule(device, chitcode);
    VkShaderModule shadowmissShaderModule = createShaderModule(device, shadowmisscode);
    VkShaderModule shadowhitShaderModule = createShaderModule(device, shadowhitcode);

    VkPipeline rayTracingPipeline;

    VkPipelineShaderStageCreateInfo shaderStages[5]{};

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

    shaderStages[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[3].stage = VK_SHADER_STAGE_MISS_BIT_KHR;
    shaderStages[3].module = shadowmissShaderModule;
    shaderStages[3].pName = "main";

    shaderStages[4].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[4].stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
    shaderStages[4].module = shadowhitShaderModule;
    shaderStages[4].pName = "main";

    VkRayTracingShaderGroupCreateInfoKHR shaderGroups[5]{};

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

    //Group 3: shadow miss
    shaderGroups[3].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[3].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    shaderGroups[3].generalShader = 3; // Index of shadow miss shader in shaderStages
    shaderGroups[3].closestHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[3].anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[3].intersectionShader = VK_SHADER_UNUSED_KHR;

    //Group 4: shadow hit
    shaderGroups[4].sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    shaderGroups[4].type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    shaderGroups[4].generalShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[4].closestHitShader = 4; // Index of shadow hit shader in shaderStages
    shaderGroups[4].anyHitShader = VK_SHADER_UNUSED_KHR;
    shaderGroups[4].intersectionShader = VK_SHADER_UNUSED_KHR;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.stageCount = (uint32_t)std::size(shaderStages);
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.groupCount = (uint32_t)std::size(shaderGroups);
    pipelineInfo.pGroups = shaderGroups;
    pipelineInfo.maxPipelineRayRecursionDepth = 2;
    pipelineInfo.layout = pipelineLayout;

    if (vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &rayTracingPipeline) != VK_SUCCESS) {
        std::cerr << "Failed to create ray tracing pipeline!" << std::endl;
        RTPipeline emptyPipeline{};
        return emptyPipeline;
    }

    std::cout << "Ray tracing pipeline created successfully!" << std::endl;

    vkDestroyShaderModule(device, raygenShaderModule, nullptr);
    vkDestroyShaderModule(device, missShaderModule, nullptr);
    vkDestroyShaderModule(device, chitShaderModule, nullptr);
    vkDestroyShaderModule(device, shadowmissShaderModule, nullptr);
    vkDestroyShaderModule(device, shadowhitShaderModule, nullptr);

    rtPipeline.pipeline = rayTracingPipeline;
    rtPipeline.layout = pipelineLayout;

    return rtPipeline;
}

void destroyPipeline(VkDevice device, RTPipeline& p)
{
    vkDestroyPipeline(device, p.pipeline, nullptr);
    vkDestroyPipelineLayout(device, p.layout, nullptr);
}
