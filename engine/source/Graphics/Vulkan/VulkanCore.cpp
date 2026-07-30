#include "Vulkan.h"

#define INVALID_MEMORY_TYPE 0xFFFFFFFF

internal uint32 FindMemoryType(VkPhysicalDevice physicalDevice, uint32 typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);

    for (uint32 i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    return INVALID_MEMORY_TYPE;
}

internal VkImageCreateInfo TextureImageInfo(uint32 width, uint32 height, VkFormat format, uint32 layers)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = layers;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (layers == 6)
    {
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    return imageInfo;
}

internal bool32 CreateImagePool(vulkan_context *context)
{
    VkImageCreateInfo probeInfo = TextureImageInfo(1, 1, VK_FORMAT_R8G8B8A8_UNORM, 1);

    VkImage probe = VK_NULL_HANDLE;
    if (vkCreateImage(context->device, &probeInfo, nullptr, &probe) != VK_SUCCESS)
    {
        DebugLog("Fail to probe image memory requirements\n");
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(context->device, probe, &memReq);
    vkDestroyImage(context->device, probe, nullptr);

    uint32 memoryType = FindMemoryType(context->physicalDevice, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memoryType == INVALID_MEMORY_TYPE)
    {
        DebugLog("Fail to find device-local memory for the image pool\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = IMAGE_POOL_SIZE;
    allocInfo.memoryTypeIndex = memoryType;

    if (vkAllocateMemory(context->device, &allocInfo, nullptr, &context->ImagePool.Memory) != VK_SUCCESS)
    {
        DebugLog("Fail to allocate image pool\n");
        return false;
    }

    context->ImagePool.Capacity = IMAGE_POOL_SIZE;
    context->ImagePool.Used     = 0;

    DebugLog("Image pool created (%llu bytes)\n", (uint64)IMAGE_POOL_SIZE);
    return true;
}

internal void DestroyImagePool(vulkan_context *context)
{
    vkFreeMemory(context->device, context->ImagePool.Memory, nullptr);
    context->ImagePool = {};
}

internal bool32 CreatePooledImage(vulkan_context *context, uint32 width, uint32 height, VkFormat format, uint32 layers, VkImage *outImage)
{
    VkImageCreateInfo imageInfo = TextureImageInfo(width, height, format, layers);

    if (vkCreateImage(context->device, &imageInfo, nullptr, outImage) != VK_SUCCESS)
    {
        DebugLog("Fail to create image\n");
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(context->device, *outImage, &memReq);

    image_memory_pool *pool = &context->ImagePool;
    VkDeviceSize offset = AlignPow2(pool->Used, memReq.alignment);

    if (offset + memReq.size > pool->Capacity)
    {
        DebugLog("Image pool out of space (used %llu, requested %llu, capacity %llu)\n", (uint64)pool->Used, (uint64)memReq.size, (uint64)pool->Capacity);
        vkDestroyImage(context->device, *outImage, nullptr);
        *outImage = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindImageMemory(context->device, *outImage, pool->Memory, offset) != VK_SUCCESS)
    {
        DebugLog("Fail to bind image into the pool\n");
        vkDestroyImage(context->device, *outImage, nullptr);
        *outImage = VK_NULL_HANDLE;
        return false;
    }

    pool->Used = offset + memReq.size;
    return true;
}

internal bool32 CreateStandaloneImage(vulkan_context *context, uint32 width, uint32 height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memoryProperties, VkImage *outImage, VkDeviceMemory *outMemory)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width  = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth  = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(context->device, &imageInfo, nullptr, outImage) != VK_SUCCESS)
    {
        DebugLog("Fail to create image\n");
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetImageMemoryRequirements(context->device, *outImage, &memReq);

    uint32 memoryType = FindMemoryType(context->physicalDevice, memReq.memoryTypeBits, memoryProperties);
    if (memoryType == INVALID_MEMORY_TYPE)
    {
        DebugLog("Fail to find suitable memory type for image\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memoryType;

    if (vkAllocateMemory(context->device, &allocInfo, nullptr, outMemory) != VK_SUCCESS)
    {
        DebugLog("Fail to allocate image memory\n");
        return false;
    }

    vkBindImageMemory(context->device, *outImage, *outMemory, 0);
    return true;
}

internal VkImageView CreateImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectMask, VkImageViewType viewType, uint32 layers)
{
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = viewType;
    viewInfo.format = format;

    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    viewInfo.subresourceRange.aspectMask = aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = layers;

    VkImageView view = VK_NULL_HANDLE;
    if (vkCreateImageView(device, &viewInfo, nullptr, &view) != VK_SUCCESS)
    {
        DebugLog("Fail to create image view\n");
        return VK_NULL_HANDLE;
    }
    return view;
}

internal VkImageView CreateColorImageView(VkDevice device, VkImage image, VkFormat format)
{
    return CreateImageView(device, image, format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
}

internal VkImageView CreateCubeImageView(VkDevice device, VkImage image, VkFormat format)
{
    return CreateImageView(device, image, format, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_VIEW_TYPE_CUBE, 6);
}

internal VkImageView CreateDepthImageView(VkDevice device, VkImage image, VkFormat format)
{
    return CreateImageView(device, image, format, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_VIEW_TYPE_2D, 1);
}

internal bool32 CreateBufferEx(vulkan_context *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags preferredProperties, VkMemoryPropertyFlags requiredProperties, VkBuffer *outBuffer, VkDeviceMemory *outMemory)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(context->device, &bufferInfo, nullptr, outBuffer) != VK_SUCCESS)
    {
        DebugLog("Fail to create buffer\n");
        return false;
    }

    VkMemoryRequirements memReq;
    vkGetBufferMemoryRequirements(context->device, *outBuffer, &memReq);

    uint32 memoryType = FindMemoryType(context->physicalDevice, memReq.memoryTypeBits, preferredProperties);
    if (memoryType == INVALID_MEMORY_TYPE && preferredProperties != requiredProperties)
    {
        DebugLog("No device-local host-visible memory, buffer falls back to system memory\n");
        memoryType = FindMemoryType(context->physicalDevice, memReq.memoryTypeBits, requiredProperties);
    }

    if (memoryType == INVALID_MEMORY_TYPE)
    {
        DebugLog("Fail to find suitable memory type for buffer\n");
        return false;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex = memoryType;

    if (vkAllocateMemory(context->device, &allocInfo, nullptr, outMemory) != VK_SUCCESS)
    {
        DebugLog("Fail to allocate buffer memory\n");
        return false;
    }

    vkBindBufferMemory(context->device, *outBuffer, *outMemory, 0);
    return true;
}

internal bool32 CreateBuffer(vulkan_context *context, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProperties, VkBuffer *outBuffer, VkDeviceMemory *outMemory)
{
    return CreateBufferEx(context, size, usage, memoryProperties, memoryProperties, outBuffer, outMemory);
}

internal bool32 CreateMappedBuffer(vulkan_context *context, VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer *outBuffer, VkDeviceMemory *outMemory, void **outMapped)
{
    VkMemoryPropertyFlags preferred = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkMemoryPropertyFlags required  = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    if (!CreateBufferEx(context, size, usage, preferred, required, outBuffer, outMemory))
    {
        return false;
    }

    vkMapMemory(context->device, *outMemory, 0, size, 0, outMapped);
    return true;
}

internal bool32 CreatePool(vulkan_context *context, gpu_pool *pool, const char *name, VkBufferUsageFlags usage, uint32 stride, uint32 capacity)
{
    pool->Stride   = stride;
    pool->Capacity = capacity;
    pool->Used     = 0;

    VkDeviceSize size = (VkDeviceSize)capacity * stride;

    if (!CreateMappedBuffer(context, size, usage, &pool->Buffer, &pool->Memory, &pool->Mapped))
    {
        DebugLog("Fail to create %s pool\n", name);
        return false;
    }

    DebugLog("%s pool created (%u entries, %llu bytes)\n", name, capacity, (uint64)size);
    return true;
}

internal void DestroyPool(vulkan_context *context, gpu_pool *pool)
{
    if (pool->Mapped)
    {
        vkUnmapMemory(context->device, pool->Memory);
    }

    vkDestroyBuffer(context->device, pool->Buffer, nullptr);
    vkFreeMemory(context->device, pool->Memory, nullptr);

    *pool = {};
}

internal void PoolWrite(gpu_pool *pool, uint32 first, const void *data, uint32 count)
{
    uint8 *destination = (uint8 *)pool->Mapped + (VkDeviceSize)first * pool->Stride;
    CopySize((memory_index)count * pool->Stride, (void *)data, destination);
}

internal VkCommandBuffer BeginSingleTimeCommands(vulkan_context *context)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = context->commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(context->device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    return cmd;
}

internal void EndSingleTimeCommands(vulkan_context *context, VkCommandBuffer cmd)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(context->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(context->graphicsQueue);

    vkFreeCommandBuffers(context->device, context->commandPool, 1, &cmd);
}

internal void TransitionImageLayout(vulkan_context *context, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout, uint32 layers)
{
    VkCommandBuffer cmd = BeginSingleTimeCommands(context);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layers;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    EndSingleTimeCommands(context, cmd);
}

internal void CopyBufferToImage(vulkan_context *context, VkBuffer buffer, VkImage image, uint32 width, uint32 height, uint32 layers)
{
    VkCommandBuffer cmd = BeginSingleTimeCommands(context);

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = layers;
    region.imageExtent.width  = width;
    region.imageExtent.height = height;
    region.imageExtent.depth  = 1;

    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    EndSingleTimeCommands(context, cmd);
}
