#include "Vulkan.h"

internal bool32 CreateTextureSampler(vulkan_context *context, VkFilter filter, VkSamplerAddressMode addressMode)
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

    if (vkCreateSampler(context->device, &samplerInfo, nullptr, &context->Sampler) != VK_SUCCESS)
    {
        DebugLog("Fail to create texture sampler\n");
        return false;
    }

    return true;
}

internal void WriteImageDescriptor(vulkan_context *context, VkDescriptorSet set, uint32 binding, uint32 arrayElement, VkImageView view)
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

internal void WriteSamplerDescriptor(vulkan_context *context, VkDescriptorSet set, uint32 binding, VkSampler sampler)
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

internal void WriteBufferDescriptor(vulkan_context *context, VkDescriptorSet set, uint32 binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize range)
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

internal bool32 CreateGlobalResources(vulkan_context *context)
{
    VkDescriptorSetLayoutBinding bindings[5] = {};
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

    VkDescriptorBindingFlags bindingFlags[5] = {};
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

    if (vkCreateDescriptorSetLayout(context->device, &layoutInfo, nullptr, &context->GlobalSet.Layout) != VK_SUCCESS)
    {
        DebugLog("Fail to create global descriptor set layout\n");
        return false;
    }

    VkDescriptorPoolSize poolSizes[4] = {};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[0].descriptorCount = MAX_TEXTURES + MAX_CUBEMAPS;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = 1;
    poolSizes[3].type            = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[3].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.poolSizeCount = (uint32)ArrayCount(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &context->GlobalDescriptorPool) != VK_SUCCESS)
    {
        DebugLog("Fail to create shared descriptor pool\n");
        return false;
    }

    if (!AllocateDescriptorSet(context, context->GlobalDescriptorPool, context->GlobalSet.Layout, &context->GlobalSet.Handle))
    {
        return false;
    }

    if (!CreateTextureSampler(context, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT))
    {
        return false;
    }
    WriteSamplerDescriptor(context, context->GlobalSet.Handle, BINDING_SAMPLER, context->Sampler);

    gpu_pool *vertexPool = &context->VertexPool;
    WriteBufferDescriptor(context, context->GlobalSet.Handle, BINDING_VERTICES, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, vertexPool->Buffer, (VkDeviceSize)vertexPool->Capacity * vertexPool->Stride);

    gpu_pool *buffer = &context->CameraBuffer;
    if (!CreatePool(context, buffer, "Camera", VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (uint32)sizeof(camera_uniforms), 1))
    {
        return false;
    }

    WriteBufferDescriptor(context, context->GlobalSet.Handle, BINDING_CAMERA, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffer->Buffer, buffer->Stride);

    return true;
}

internal void DestroyGlobalResources(vulkan_context *context)
{
    DestroyPool(context, &context->CameraBuffer);

    vkDestroySampler(context->device, context->Sampler, nullptr);
    vkDestroyDescriptorPool(context->device, context->GlobalDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(context->device, context->GlobalSet.Layout, nullptr);

    context->Sampler              = VK_NULL_HANDLE;
    context->GlobalDescriptorPool = VK_NULL_HANDLE;
    context->GlobalSet            = {};
}

internal void BindGlobalSet(VkCommandBuffer cmd, vulkan_context *context, render_pipeline *pipeline)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, 0, 1, &context->GlobalSet.Handle, 0, nullptr);
}

internal void *CameraUniforms(vulkan_context *context)
{
    return context->CameraBuffer.Mapped;
}

