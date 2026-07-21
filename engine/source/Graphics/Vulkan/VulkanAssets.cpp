#include "Vulkan.h"

internal bool32 CreateMesh(vulkan_context *context, gpu_mesh *mesh, real32 *vertices, uint32 vertexCount)
{
    VkDeviceSize bufferSize = (VkDeviceSize)vertexCount * KBN_VERTEX_FLOATS * sizeof(real32);
    mesh->VertexCount = vertexCount;

    if (!CreateBuffer(context, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &mesh->VertexBuffer, &mesh->VertexBufferMemory))
    {
        DebugLog("Fail to create mesh vertex buffer\n");
        return false;
    }

    void *mapped = nullptr;
    vkMapMemory(context->device, mesh->VertexBufferMemory, 0, bufferSize, 0, &mapped);
    CopySize(bufferSize, vertices, mapped);
    vkUnmapMemory(context->device, mesh->VertexBufferMemory);

    return true;
}

internal bool32 CreateTexture(vulkan_context *context, gpu_texture *texture, uint32 textureID, void *pixels, uint32 width, uint32 height, VkFormat format)
{
    VkDeviceSize imageSize = (VkDeviceSize)width * (VkDeviceSize)height * 4;

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
    CopySize(imageSize, pixels, mapped);
    vkUnmapMemory(context->device, stagingMemory);

    if (!CreateImage(context, width, height, format, VK_IMAGE_TILING_OPTIMAL,
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &texture->Image, &texture->Memory))
    {
        vkDestroyBuffer(context->device, staging, nullptr);
        vkFreeMemory(context->device, stagingMemory, nullptr);
        return false;
    }

    TransitionImageLayout(context, texture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(context, staging, texture->Image, width, height);
    TransitionImageLayout(context, texture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(context->device, staging, nullptr);
    vkFreeMemory(context->device, stagingMemory, nullptr);

    texture->View = CreateColorImageView(context->device, texture->Image, format);

    render_pipeline *pipeline = context->PrimitivePipeline;
    if (!pipeline)
    {
        DebugLog("Texture upload skipped: pipeline 'primitive' not ready\n");
        return false;
    }
    WriteImageDescriptor(context, pipeline->Sets[Set_PerMaterial], 0, textureID, texture->View);

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
            if (entry->MeshID >= MAX_MESHES || context->Meshes[entry->MeshID].VertexBuffer != VK_NULL_HANDLE)
            {
                DebugLog("Mesh %u skipped (slot bad or busy)\n", entry->MeshID);
                continue;
            }

            if (CreateMesh(context, &context->Meshes[entry->MeshID], entry->Vertices, entry->VertexCount) &&
                entry->MeshID >= context->MeshCount)
            {
                context->MeshCount = entry->MeshID + 1;
            }
        }
        else if (*header == LoadTexture)
        {
            command_load_texture *entry = (command_load_texture *)header;
            if (entry->TextureID >= MAX_TEXTURES || context->Textures[entry->TextureID].Image != VK_NULL_HANDLE)
            {
                DebugLog("Texture %u skipped (slot bad or busy)\n", entry->TextureID);
                continue;
            }

            VkFormat format = entry->SRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;
            if (CreateTexture(context, &context->Textures[entry->TextureID], entry->TextureID, entry->Pixels, entry->Width, entry->Height, format) &&
                entry->TextureID >= context->TextureCount)
            {
                context->TextureCount = entry->TextureID + 1;
            }
        }
    }
}
