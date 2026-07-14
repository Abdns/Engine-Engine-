#include "Vulkan.h"

internal bool32 CreateMesh(vulkan_context *context, uint32 meshIndex, real32 *vertices, uint32 floatCount)
{
    gpu_mesh *mesh = &context->Meshes[meshIndex];
    VkDeviceSize bufferSize = floatCount * sizeof(real32);
    mesh->VertexCount = floatCount / 8;

    if (!CreateBuffer(context, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                      &mesh->VertexBuffer, &mesh->VertexBufferMemory))
    {
        DebugLog("Fail to create mesh vertex buffer\n");
        return false;
    }

    void *mapped = nullptr;
    vkMapMemory(context->device, mesh->VertexBufferMemory, 0, bufferSize, 0, &mapped);
    uint8 *src = (uint8 *)vertices;
    uint8 *dst = (uint8 *)mapped;
    for (VkDeviceSize i = 0; i < bufferSize; ++i)
    {
        dst[i] = src[i];
    }
    vkUnmapMemory(context->device, mesh->VertexBufferMemory);

    return true;
}

internal bool32 CreateMeshes(vulkan_context *context)
{
    real32 s = 0.5f;

    real32 CubeVertices[] =
    {
         s,-s,-s, 1,0,0, 0,0,    s, s,-s, 1,0,0, 1,0,    s, s, s, 1,0,0, 1,1,
         s,-s,-s, 1,0,0, 0,0,    s, s, s, 1,0,0, 1,1,    s,-s, s, 1,0,0, 0,1,
        -s,-s, s, 0,1,0, 0,0,   -s, s, s, 0,1,0, 1,0,   -s, s,-s, 0,1,0, 1,1,
        -s,-s, s, 0,1,0, 0,0,   -s, s,-s, 0,1,0, 1,1,   -s,-s,-s, 0,1,0, 0,1,
        -s, s,-s, 0,0,1, 0,0,    s, s,-s, 0,0,1, 1,0,    s, s, s, 0,0,1, 1,1,
        -s, s,-s, 0,0,1, 0,0,    s, s, s, 0,0,1, 1,1,   -s, s, s, 0,0,1, 0,1,
        -s,-s, s, 1,1,0, 0,0,    s,-s, s, 1,1,0, 1,0,    s,-s,-s, 1,1,0, 1,1,
        -s,-s, s, 1,1,0, 0,0,    s,-s,-s, 1,1,0, 1,1,   -s,-s,-s, 1,1,0, 0,1,
        -s,-s, s, 1,0,1, 0,0,    s,-s, s, 1,0,1, 1,0,    s, s, s, 1,0,1, 1,1,
        -s,-s, s, 1,0,1, 0,0,    s, s, s, 1,0,1, 1,1,   -s, s, s, 1,0,1, 0,1,
         s,-s,-s, 0,1,1, 0,0,   -s,-s,-s, 0,1,1, 1,0,   -s, s,-s, 0,1,1, 1,1,
         s,-s,-s, 0,1,1, 0,0,   -s, s,-s, 0,1,1, 1,1,    s, s,-s, 0,1,1, 0,1,
    };

    real32 PyramidVertices[] =
    {
         0, s, 0, 1,0,0, 0.5f,0,   -s,-s,-s, 1,0,0, 0,1,    s,-s,-s, 1,0,0, 1,1,
         0, s, 0, 0,1,0, 0.5f,0,    s,-s,-s, 0,1,0, 0,1,    s,-s, s, 0,1,0, 1,1,
         0, s, 0, 0,0,1, 0.5f,0,    s,-s, s, 0,0,1, 0,1,   -s,-s, s, 0,0,1, 1,1,
         0, s, 0, 1,1,0, 0.5f,0,   -s,-s, s, 1,1,0, 0,1,   -s,-s,-s, 1,1,0, 1,1,
        -s,-s,-s, 0.6f,0.6f,0.6f, 0,0,    s,-s,-s, 0.6f,0.6f,0.6f, 1,0,    s,-s, s, 0.6f,0.6f,0.6f, 1,1,
        -s,-s,-s, 0.6f,0.6f,0.6f, 0,0,    s,-s, s, 0.6f,0.6f,0.6f, 1,1,   -s,-s, s, 0.6f,0.6f,0.6f, 0,1,
    };

    context->MeshCount = 0;
    if (!CreateMesh(context, Mesh_Cube,    CubeVertices,    (uint32)ArrayCount(CubeVertices)))    return false;
    if (!CreateMesh(context, Mesh_Pyramid, PyramidVertices, (uint32)ArrayCount(PyramidVertices))) return false;
    context->MeshCount = Mesh_Count;

    DebugLog("Meshes created (%u)\n", context->MeshCount);
    return true;
}

internal bool32 CreateTexture(vulkan_context *context, gpu_texture *texture, void *pixels, uint32 width, uint32 height, VkFormat format)
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
    uint8 *src = (uint8 *)pixels;
    uint8 *dst = (uint8 *)mapped;
    for (VkDeviceSize i = 0; i < imageSize; ++i)
    {
        dst[i] = src[i];
    }
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

    CreateTextureDescriptorSet(context, &context->Pipeline, texture);

    return true;
}

internal bool32 LoadTexture(vulkan_context *context, gpu_texture *texture, const char *filename, VkFormat format)
{
    uint32 arenaSize = (uint32)Megabytes(16);
    memory_arena arena;
    InitializeArena(&arena, arenaSize, VirtualAlloc(0, arenaSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));

    loaded_bitmap bmp = LoadTGA(&arena, filename);

    bool32 ok = false;
    if (bmp.Pixels)
    {
        ok = CreateTexture(context, texture, bmp.Pixels, bmp.Width, bmp.Height, format);
    }
    else
    {
        DebugLog("LoadTexture: '%s' not loaded\n", filename);
    }

    VirtualFree(arena.Base, 0, MEM_RELEASE);
    return ok;
}

internal bool32 CreateTextures(vulkan_context *context)
{
    context->TextureCount = 0;

    if (!LoadTexture(context, &context->Textures[Texture_Photo], "assets/images/TestImage.tga", VK_FORMAT_R8G8B8A8_SRGB))
    {
        return false;
    }

    uint32 checker[64 * 64];
    for (uint32 y = 0; y < 64; ++y)
    {
        for (uint32 x = 0; x < 64; ++x)
        {
            bool32 on = (((x / 8) + (y / 8)) & 1) != 0;
            checker[y * 64 + x] = on ? 0xFFFFFFFFu : 0xFF404040u;
        }
    }
    if (!CreateTexture(context, &context->Textures[Texture_Checker], checker, 64, 64, VK_FORMAT_R8G8B8A8_UNORM))
    {
        return false;
    }

    context->TextureCount = Texture_Count;

    DebugLog("Textures created (%u)\n", context->TextureCount);
    return true;
}
