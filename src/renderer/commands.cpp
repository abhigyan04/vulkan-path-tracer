#include "commands.hpp"
#include <iostream>


void recordCommandBuffer(VkDevice device, VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t imageIndex, VkPipeline pipeline, VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkImage rtImage, VkImage accumImage, std::vector<VkImage> swapChainImages, const SBT& sbt, VkExtent2D extent, bool cameraMoved)
{
    auto vkCmdTraceRaysKHR =
        (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        std::cerr << "Failed to begin recording command buffer!" << std::endl;
        return;
    }

    //Bind + Trace
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            pipelineLayout,
                            0,
                            1,
                            &descriptorSet,
                            0, nullptr);

    if(cameraMoved)
    {
        VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 0.0f};

        VkImageSubresourceRange clearRange{};
        clearRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        clearRange.baseMipLevel = 0;
        clearRange.levelCount = 1;
        clearRange.baseArrayLayer = 0;
        clearRange.layerCount = 1;

        vkCmdClearColorImage(commandBuffer, accumImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &clearRange);
    }
                        
    PushConstants pushConstants{};
    pushConstants.frame = frameIndex;

    vkCmdPushConstants(commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0,
        sizeof(PushConstants),
        &pushConstants);

    vkCmdTraceRaysKHR(commandBuffer,
        &sbt.raygenRegion,
        &sbt.missRegion,
        &sbt.hitRegion,
        &sbt.callableRegion,
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
}
