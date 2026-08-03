#include "Vulkan.h"

global_variable vulkan_resources GlobalResources;

internal bool32 CreateTextureSampler(vulkan_context *context, vulkan_resources *res, VkFilter filter, VkSamplerAddressMode addressMode)
{
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;

    if (vkCreateSampler(context->device, &samplerInfo, nullptr, &res->Sampler) != VK_SUCCESS)
    {
        DebugLog("Fail to create texture sampler\n");
        return false;
    }

    return true;
}

internal void UpdateImageDescriptorInSet(vulkan_context *context, VkDescriptorSet set, uint32 binding, uint32 arrayElement, VkImageView view)
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = view;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = arrayElement;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(context->device, 1, &write, 0, nullptr);
}

internal void UpdateSamplerDescriptorInSet(vulkan_context *context, VkDescriptorSet set, uint32 binding, VkSampler sampler)
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(context->device, 1, &write, 0, nullptr);
}

internal void UpdateBufferDescriptorInSet(vulkan_context *context, VkDescriptorSet set, uint32 binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize range)
{
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = range;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(context->device, 1, &write, 0, nullptr);
}

internal bool32 AllocateDescriptorSet(vulkan_context *context, VkDescriptorPool pool, VkDescriptorSetLayout layout, VkDescriptorSet *outSet)
{
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    if (vkAllocateDescriptorSets(context->device, &allocInfo, outSet) != VK_SUCCESS)
    {
        DebugLog("Fail to allocate descriptor set\n");
        return false;
    }

    return true;
}

internal bool32 CreateGlobalResources(vulkan_context *context, vulkan_resources *res)
{
    VkDescriptorSetLayoutBinding bindings[6] = {};
    bindings[0].binding         = BINDING_TEXTURES;
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[0].descriptorCount = MAX_TEXTURES;
    bindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[1].binding         = BINDING_SAMPLER;
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[2].binding         = BINDING_VERTICES;
    bindings[2].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT;

    bindings[3].binding         = BINDING_CAMERA;
    bindings[3].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[4].binding         = BINDING_CUBEMAPS;
    bindings[4].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[4].descriptorCount = MAX_CUBEMAPS;
    bindings[4].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    bindings[5].binding         = BINDING_MATERIALS;
    bindings[5].descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[5].descriptorCount = 1;
    bindings[5].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorBindingFlags bindingFlags[6] = {};
    bindingFlags[0] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
    bindingFlags[4] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
    flagsInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
    flagsInfo.bindingCount  = (uint32)ArrayCount(bindingFlags);
    flagsInfo.pBindingFlags = bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.pNext        = &flagsInfo;
    layoutInfo.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.bindingCount = (uint32)ArrayCount(bindings);
    layoutInfo.pBindings    = bindings;

    if (vkCreateDescriptorSetLayout(context->device, &layoutInfo, nullptr, &res->GlobalSet.Layout) != VK_SUCCESS)
    {
        DebugLog("Fail to create global descriptor set layout\n");
        return false;
    }

    VkDescriptorPoolSize poolSizes[4] = {};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[0].descriptorCount = MAX_TEXTURES + MAX_CUBEMAPS + Pipeline_Count;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = 1;
    poolSizes[3].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[3].descriptorCount = 2;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.poolSizeCount = (uint32)ArrayCount(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1 + Pipeline_Count;

    if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &res->DescriptorPool) != VK_SUCCESS)
    {
        DebugLog("Fail to create shared descriptor pool\n");
        return false;
    }

    if (!AllocateDescriptorSet(context, res->DescriptorPool, res->GlobalSet.Layout, &res->GlobalSet.Handle))
    {
        return false;
    }

    if (!CreateTextureSampler(context, res, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT))
    {
        return false;
    }
    UpdateSamplerDescriptorInSet(context, res->GlobalSet.Handle, BINDING_SAMPLER, res->Sampler);

    UpdateBufferDescriptorInSet(context, res->GlobalSet.Handle, BINDING_VERTICES, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, res->VertexBuffer.Buffer, (VkDeviceSize)res->VertexBuffer.Capacity * res->VertexBuffer.Stride);

    res->CameraBuffer = CreateBuffer(context, "Camera", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (uint32)sizeof(camera_uniforms), 1);
    if (res->CameraBuffer.Buffer == VK_NULL_HANDLE)
    {
        return false;
    }

    UpdateBufferDescriptorInSet(context, res->GlobalSet.Handle, BINDING_CAMERA, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, res->CameraBuffer.Buffer, res->CameraBuffer.Stride);

    res->MaterialBuffer = CreateBuffer(context, "Material", VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, (uint32)sizeof(gpu_material), MAX_MATERIALS);
    if (res->MaterialBuffer.Buffer == VK_NULL_HANDLE)
    {
        return false;
    }

    UpdateBufferDescriptorInSet(context, res->GlobalSet.Handle, BINDING_MATERIALS, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, res->MaterialBuffer.Buffer, (VkDeviceSize)res->MaterialBuffer.Capacity * res->MaterialBuffer.Stride);

    return true;
}

internal void DestroyGlobalResources(vulkan_context *context, vulkan_resources *res)
{
    DestroyBuffer(context, &res->MaterialBuffer);
    DestroyBuffer(context, &res->CameraBuffer);

    vkDestroySampler(context->device, res->Sampler, nullptr);
    vkDestroyDescriptorPool(context->device, res->DescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(context->device, res->GlobalSet.Layout, nullptr);

    res->Sampler        = VK_NULL_HANDLE;
    res->DescriptorPool = VK_NULL_HANDLE;
    res->GlobalSet      = {};
}

internal bool32 CreatePipelineSet(vulkan_context *context, vulkan_resources *res, descriptor_set *set)
{
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding         = BINDING_PIPELINE_IMAGE;
    binding.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings    = &binding;

    if (vkCreateDescriptorSetLayout(context->device, &layoutInfo, nullptr, &set->Layout) != VK_SUCCESS)
    {
        DebugLog("Fail to create pipeline descriptor set layout\n");
        return false;
    }

    return AllocateDescriptorSet(context, res->DescriptorPool, set->Layout, &set->Handle);
}

internal void DestroyPipelineSet(vulkan_context *context, descriptor_set *set)
{
    vkDestroyDescriptorSetLayout(context->device, set->Layout, nullptr);

    *set = {};
}

internal void BindGlobalSet(VkCommandBuffer cmd, vulkan_resources *res, VkPipelineLayout layout)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &res->GlobalSet.Handle, 0, nullptr);
}

internal void BindPipelineSet(VkCommandBuffer cmd, descriptor_set *set, VkPipelineLayout layout)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 1, 1, &set->Handle, 0, nullptr);
}

internal void *CameraUniforms(vulkan_resources *res)
{
    return res->CameraBuffer.Mapped;
}

internal gpu_mesh *ResolveMesh(vulkan_resources *res, uint32 MeshHandle)
{
    if (!MeshHandle || MeshHandle > MAX_MESHES)
    {
        return 0;
    }

    return res->Meshes + (MeshHandle - 1);
}

internal bool32 WriteMesh(uint32 MeshHandle, void *Vertices, uint32 VertexCount, uint32 *Indices, uint32 IndexCount)
{
    vulkan_resources *res = &GlobalResources;

    gpu_mesh *mesh = ResolveMesh(res, MeshHandle);
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

    gpu_buffer *vertexBuffer = &res->VertexBuffer;
    gpu_buffer *indexBuffer  = &res->IndexBuffer;

    if (!VertexCount || !IndexCount ||
        VertexCount > vertexBuffer->Capacity - vertexBuffer->Used ||
        IndexCount  > indexBuffer->Capacity  - indexBuffer->Used)
    {
        DebugLog("Geometry buffers out of space for mesh id %u (%u vertices, %u indices)\n", MeshHandle, VertexCount, IndexCount);
        return false;
    }

    BufferWrite(vertexBuffer, vertexBuffer->Used, Vertices, VertexCount);
    BufferWrite(indexBuffer,  indexBuffer->Used,  Indices,  IndexCount);

    mesh->FirstVertex = vertexBuffer->Used;
    mesh->VertexCount = VertexCount;
    mesh->FirstIndex  = indexBuffer->Used;
    mesh->IndexCount  = IndexCount;

    vertexBuffer->Used += VertexCount;
    indexBuffer->Used  += IndexCount;

    return true;
}

internal bool32 WriteTexture(uint32 TextureHandle, void *Pixels, uint32 Width, uint32 Height, uint32 SRGB, texture_format TextureFormat)
{
    vulkan_context   *context = &GlobalVulkan;
    vulkan_resources *res     = &GlobalResources;

    if (!TextureHandle || TextureHandle > MAX_TEXTURES)
    {
        DebugLog("Texture id %u out of range (max %d)\n", TextureHandle, MAX_TEXTURES);
        return false;
    }

    uint32 slot = TextureHandle - 1;

    gpu_texture *texture = &res->Textures[slot];
    if (texture->Image != VK_NULL_HANDLE)
    {
        DebugLog("Texture id %u already taken\n", TextureHandle);
        return false;
    }

    VkFormat format = (TextureFormat == TextureFormat_RGBA16F) ? VK_FORMAT_R16G16B16A16_SFLOAT : (SRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM);

    VkDeviceSize imageSize = (VkDeviceSize)Width * (VkDeviceSize)Height * TextureFormatBytes(TextureFormat);

    VkBuffer       staging       = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!CreateStagingBuffer(context, imageSize, Pixels, &staging, &stagingMemory))
    {
        return false;
    }

    if (!ArenaPushImage(context, &res->ImageArena, Width, Height, format, 1, &texture->Image))
    {
        FreeBuffer(context, &staging, &stagingMemory);
        return false;
    }

    TransitionImageLayout(context, texture->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1);
    CopyBufferToImage(context, staging, texture->Image, Width, Height, 1);
    TransitionImageLayout(context, texture->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 1);

    FreeBuffer(context, &staging, &stagingMemory);

    texture->View = CreateColorImageView(context->device, texture->Image, format);

    UpdateImageDescriptorInSet(context, res->GlobalSet.Handle, BINDING_TEXTURES, slot, texture->View);

    return true;
}

internal bool32 WriteCubemap(uint32 CubemapHandle, void *Pixels, uint32 FaceSize, texture_format TextureFormat)
{
    vulkan_context   *context = &GlobalVulkan;
    vulkan_resources *res     = &GlobalResources;

    if (!CubemapHandle || CubemapHandle > MAX_CUBEMAPS)
    {
        DebugLog("Cubemap id %u out of range (max %d)\n", CubemapHandle, MAX_CUBEMAPS);
        return false;
    }

    uint32 slot = CubemapHandle - 1;

    gpu_texture *cube = &res->Cubemaps[slot];
    if (cube->Image != VK_NULL_HANDLE)
    {
        DebugLog("Cubemap id %u already taken\n", CubemapHandle);
        return false;
    }

    VkFormat format = (TextureFormat == TextureFormat_RGBA16F) ? VK_FORMAT_R16G16B16A16_SFLOAT : VK_FORMAT_R8G8B8A8_UNORM;

    VkDeviceSize imageSize = (VkDeviceSize)FaceSize * FaceSize * 6 * TextureFormatBytes(TextureFormat);

    VkBuffer       staging       = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!CreateStagingBuffer(context, imageSize, Pixels, &staging, &stagingMemory))
    {
        return false;
    }

    if (!ArenaPushImage(context, &res->ImageArena, FaceSize, FaceSize, format, 6, &cube->Image))
    {
        FreeBuffer(context, &staging, &stagingMemory);
        return false;
    }

    TransitionImageLayout(context, cube->Image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6);
    CopyBufferToImage(context, staging, cube->Image, FaceSize, FaceSize, 6);
    TransitionImageLayout(context, cube->Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 6);

    FreeBuffer(context, &staging, &stagingMemory);

    cube->View = CreateCubeImageView(context->device, cube->Image, format);

    UpdateImageDescriptorInSet(context, res->GlobalSet.Handle, BINDING_CUBEMAPS, slot, cube->View);

    return true;
}

internal bool32 WriteMaterial(command_load_material *Description)
{
    vulkan_resources *res = &GlobalResources;

    uint32 MaterialHandle = Description->MaterialHandle;
    if (!MaterialHandle || MaterialHandle > MAX_MATERIALS)
    {
        DebugLog("Material id %u out of range (max %d)\n", MaterialHandle, MAX_MATERIALS);
        return false;
    }

    if (Description->Pipeline >= Pipeline_MeshCount)
    {
        DebugLog("Material %u references pipeline %d, which does not draw meshes\n", MaterialHandle, Description->Pipeline);
        return false;
    }

    uint32 textureSlot = 0;
    if (Description->TextureHandle && Description->TextureHandle <= MAX_TEXTURES && res->Textures[Description->TextureHandle - 1].View)
    {
        textureSlot = Description->TextureHandle - 1;
    }
    else
    {
        DebugLog("Material %u references missing texture %u\n", MaterialHandle, Description->TextureHandle);
    }

    uint32 slot = MaterialHandle - 1;

    material_state *state = res->MaterialStates + slot;
    state->Pipeline   = Description->Pipeline;
    state->CullMode   = Description->CullMode;
    state->BlendMode  = Description->BlendMode;
    state->DepthTest  = Description->DepthTest;
    state->DepthWrite = Description->DepthWrite;

    gpu_material *materials = (gpu_material *)res->MaterialBuffer.Mapped;
    materials[slot].BaseColor    = Description->BaseColor;
    materials[slot].TextureIndex = textureSlot;

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
        else if (*header == Load_Material)
        {
            command_load_material *entry = (command_load_material *)header;
            WriteMaterial(entry);
        }
    }

    commands->LoadCount = 0;
}
