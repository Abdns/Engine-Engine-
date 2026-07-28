#include "Vulkan.h"

internal bool32 CreateGeometryPools(vulkan_context *context)
{
    if (!CreatePool(context, &context->VertexPool, "Vertex", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, (uint32)sizeof(vertex), VERTEX_POOL_VERTICES))
    {
        return false;
    }

    return CreatePool(context, &context->IndexPool, "Index", VK_BUFFER_USAGE_INDEX_BUFFER_BIT, (uint32)sizeof(uint32), INDEX_POOL_INDICES);
}

internal void DestroyGeometryPools(vulkan_context *context)
{
    DestroyPool(context, &context->IndexPool);
    DestroyPool(context, &context->VertexPool);
}

internal PLATFORM_GET_GPU_LIMITS(VulkanGetGpuLimits)
{
    vulkan_context *context = &GlobalVulkan;

    gpu_limits result;
    result.MaxVertices = context->VertexPool.Capacity;
    result.MaxIndices  = context->IndexPool.Capacity;
    result.MaxTextures = MAX_TEXTURES;

    return result;
}

internal PLATFORM_WRITE_VERTICES(VulkanWriteVertices)
{
    return PoolWrite(&GlobalVulkan.VertexPool, "Vertex", FirstVertex, Data, VertexCount);
}

internal PLATFORM_WRITE_INDICES(VulkanWriteIndices)
{
    return PoolWrite(&GlobalVulkan.IndexPool, "Index", FirstIndex, Data, IndexCount);
}

internal PLATFORM_WRITE_TEXTURE(VulkanWriteTexture)
{
    vulkan_context *context = &GlobalVulkan;

    if (Slot >= MAX_TEXTURES)
    {
        DebugLog("Texture slot %u out of range (max %d)\n", Slot, MAX_TEXTURES);
        return false;
    }

    gpu_texture *texture = &context->Textures[Slot];
    if (texture->Image != VK_NULL_HANDLE)
    {
        DebugLog("Texture slot %u already taken\n", Slot);
        return false;
    }

    VkFormat     format    = SRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
    VkDeviceSize imageSize = (VkDeviceSize)Width * (VkDeviceSize)Height * 4;

    VkBuffer       staging       = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!CreateBuffer(context, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &staging, &stagingMemory))
    {
        return false;
    }

    void *mapped = nullptr;
    vkMapMemory(context->device, stagingMemory, 0, imageSize, 0, &mapped);
    CopySize(imageSize, Pixels, mapped);
    vkUnmapMemory(context->device, stagingMemory);

    if (!CreatePooledImage(context, Width, Height, format, &texture->Image))
    {
        vkDestroyBuffer(context->device, staging, nullptr);
        vkFreeMemory(context->device, stagingMemory, nullptr);
        return false;
    }

    TransitionImageLayout(context, texture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(context, staging, texture->Image, Width, Height);
    TransitionImageLayout(context, texture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(context->device, staging, nullptr);
    vkFreeMemory(context->device, stagingMemory, nullptr);

    texture->View = CreateColorImageView(context->device, texture->Image, format);

    WriteImageDescriptor(context, context->GlobalSet.Handle, BINDING_TEXTURES, Slot, texture->View);

    return true;
}

