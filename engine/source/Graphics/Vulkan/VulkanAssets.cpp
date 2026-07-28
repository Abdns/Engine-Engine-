#include "Vulkan.h"

internal bool32 CreateGeometryPools(vulkan_context *context)
{
    if (!CreatePool(context, &context->VertexPool, "Vertex", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, (uint32)sizeof(vertex), RENDER_MAX_VERTICES))
    {
        return false;
    }

    return CreatePool(context, &context->IndexPool, "Index", VK_BUFFER_USAGE_INDEX_BUFFER_BIT, (uint32)sizeof(uint32), RENDER_MAX_INDICES);
}

internal void DestroyGeometryPools(vulkan_context *context)
{
    DestroyPool(context, &context->IndexPool);
    DestroyPool(context, &context->VertexPool);
}

internal bool32 WriteMesh(mesh_handle Where, real32 *Vertices, uint32 *Indices)
{
    return PoolWrite(&GlobalVulkan.VertexPool, "Vertex", Where.FirstVertex, Vertices, Where.VertexCount) &&
           PoolWrite(&GlobalVulkan.IndexPool,  "Index",  Where.FirstIndex,  Indices,  Where.IndexCount);
}

internal bool32 WriteTexture(texture_handle Where, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB)
{
    vulkan_context *context = &GlobalVulkan;

    if (Where.Index >= MAX_TEXTURES)
    {
        DebugLog("Texture slot %u out of range (max %d)\n", Where.Index, MAX_TEXTURES);
        return false;
    }

    gpu_texture *texture = &context->Textures[Where.Index];
    if (texture->Image != VK_NULL_HANDLE)
    {
        DebugLog("Texture slot %u already taken\n", Where.Index);
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

    WriteImageDescriptor(context, context->GlobalSet.Handle, BINDING_TEXTURES, Where.Index, texture->View);

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
            WriteMesh(entry->Where, entry->Vertices, entry->Indices);
        }
        else if (*header == Load_Texture)
        {
            command_load_texture *entry = (command_load_texture *)header;
            WriteTexture(entry->Where, entry->Pixels, entry->Width, entry->Height, entry->SRGB);
        }
    }

    commands->LoadCount = 0;
}
