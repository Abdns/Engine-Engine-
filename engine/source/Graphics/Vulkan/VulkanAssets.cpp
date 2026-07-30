#include "Vulkan.h"

internal bool32 CreateGeometryPools(vulkan_context *context)
{
    if (!CreatePool(context, &context->VertexPool, "Vertex", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, (uint32)sizeof(vertex), MAX_POOL_VERTICES))
    {
        return false;
    }

    return CreatePool(context, &context->IndexPool, "Index", VK_BUFFER_USAGE_INDEX_BUFFER_BIT, (uint32)sizeof(uint32), MAX_POOL_INDICES);
}

internal void DestroyGeometryPools(vulkan_context *context)
{
    DestroyPool(context, &context->IndexPool);
    DestroyPool(context, &context->VertexPool);
}

internal gpu_mesh *ResolveMesh(vulkan_context *context, uint32 MeshHandle)
{
    if (!MeshHandle || MeshHandle > MAX_MESHES)
    {
        return 0;
    }

    return context->Meshes + (MeshHandle - 1);
}

internal bool32 WriteMesh(uint32 MeshHandle, void *Vertices, uint32 VertexCount, uint32 *Indices, uint32 IndexCount)
{
    vulkan_context *context = &GlobalVulkan;

    gpu_mesh *mesh = ResolveMesh(context, MeshHandle);
    if (!mesh)
    {
        DebugLog("Mesh id %u out of range (max %d)\n", MeshHandle, MAX_MESHES);
        return false;
    }

    if (mesh->IndexCount)
    {
        DebugLog("Mesh id %u already taken\n", MeshHandle);
        return false;
    }

    gpu_pool *vertexPool = &context->VertexPool;
    gpu_pool *indexPool  = &context->IndexPool;

    if (!VertexCount || !IndexCount ||
        VertexCount > vertexPool->Capacity - vertexPool->Used ||
        IndexCount  > indexPool->Capacity  - indexPool->Used)
    {
        DebugLog("Geometry pools out of space for mesh id %u (%u vertices, %u indices)\n", MeshHandle, VertexCount, IndexCount);
        return false;
    }

    PoolWrite(vertexPool, vertexPool->Used, Vertices, VertexCount);
    PoolWrite(indexPool,  indexPool->Used,  Indices,  IndexCount);

    mesh->FirstVertex = vertexPool->Used;
    mesh->VertexCount = VertexCount;
    mesh->FirstIndex  = indexPool->Used;
    mesh->IndexCount  = IndexCount;

    vertexPool->Used += VertexCount;
    indexPool->Used  += IndexCount;

    return true;
}

internal bool32 WriteTexture(uint32 TextureHandle, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB, texture_format TextureFormat)
{
    vulkan_context *context = &GlobalVulkan;

    if (!TextureHandle || TextureHandle > MAX_TEXTURES)
    {
        DebugLog("Texture id %u out of range (max %d)\n", TextureHandle, MAX_TEXTURES);
        return false;
    }

    uint32 slot = TextureHandle - 1;

    gpu_texture *texture = &context->Textures[slot];
    if (texture->Image != VK_NULL_HANDLE)
    {
        DebugLog("Texture id %u already taken\n", TextureHandle);
        return false;
    }

    VkFormat format = (TextureFormat == TextureFormat_RGBA16F) ? VK_FORMAT_R16G16B16A16_SFLOAT : (SRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM);

    VkDeviceSize imageSize = (VkDeviceSize)Width * (VkDeviceSize)Height * TextureFormatBytes(TextureFormat);

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

    if (!CreatePooledImage(context, Width, Height, format, 1, &texture->Image))
    {
        vkDestroyBuffer(context->device, staging, nullptr);
        vkFreeMemory(context->device, stagingMemory, nullptr);
        return false;
    }

    TransitionImageLayout(context, texture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
    CopyBufferToImage(context, staging, texture->Image, Width, Height, 1);
    TransitionImageLayout(context, texture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

    vkDestroyBuffer(context->device, staging, nullptr);
    vkFreeMemory(context->device, stagingMemory, nullptr);

    texture->View = CreateColorImageView(context->device, texture->Image, format);

    WriteImageDescriptor(context, context->GlobalSet.Handle, BINDING_TEXTURES, slot, texture->View);

    return true;
}

internal bool32 WriteCubemap(uint32 CubemapHandle, void *Pixels, uint32 FaceSize, texture_format TextureFormat)
{
    vulkan_context *context = &GlobalVulkan;

    if (!CubemapHandle || CubemapHandle > MAX_CUBEMAPS)
    {
        DebugLog("Cubemap id %u out of range (max %d)\n", CubemapHandle, MAX_CUBEMAPS);
        return false;
    }

    uint32 slot = CubemapHandle - 1;

    gpu_texture *cube = &context->Cubemaps[slot];
    if (cube->Image != VK_NULL_HANDLE)
    {
        DebugLog("Cubemap id %u already taken\n", CubemapHandle);
        return false;
    }

    VkFormat format = (TextureFormat == TextureFormat_RGBA16F) ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;

    VkDeviceSize imageSize = (VkDeviceSize)FaceSize * FaceSize * 6 * TextureFormatBytes(TextureFormat);

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

    if (!CreatePooledImage(context, FaceSize, FaceSize, format, 6, &cube->Image))
    {
        vkDestroyBuffer(context->device, staging, nullptr);
        vkFreeMemory(context->device, stagingMemory, nullptr);
        return false;
    }

    TransitionImageLayout(context, cube->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6);
    CopyBufferToImage(context, staging, cube->Image, FaceSize, FaceSize, 6);
    TransitionImageLayout(context, cube->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6);

    vkDestroyBuffer(context->device, staging, nullptr);
    vkFreeMemory(context->device, stagingMemory, nullptr);

    cube->View = CreateCubeImageView(context->device, cube->Image, format);

    WriteImageDescriptor(context, context->GlobalSet.Handle, BINDING_CUBEMAPS, slot, cube->View);

    return true;
}

internal void ProcessLoadCommands(vulkan_context *context, render_commands *commands)
{
    if (!commands->LoadCount)
    {
        return;
    }

    uint32 offset = 0;
    for (command_type *header = NextRenderCommand(commands, &offset); header; header = NextRenderCommand(commands, &offset))
    {
        if (*header == Load_Mesh)
        {
            command_load_mesh *entry = (command_load_mesh *)header;
            WriteMesh(entry->MeshHandle, entry->Vertices, entry->VertexCount, entry->Indices, entry->IndexCount);
        }
        else if (*header == Load_Texture)
        {
            command_load_texture *entry = (command_load_texture *)header;
            WriteTexture(entry->TextureHandle, entry->Pixels, entry->Width, entry->Height, entry->SRGB, entry->Format);
        }
        else if (*header == Load_Cubemap)
        {
            command_load_cubemap *entry = (command_load_cubemap *)header;
            WriteCubemap(entry->CubemapHandle, entry->Pixels, entry->FaceSize, entry->Format);
        }
    }

    commands->LoadCount = 0;
}
