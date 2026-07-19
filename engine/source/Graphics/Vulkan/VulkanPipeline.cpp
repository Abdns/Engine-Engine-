#include "Vulkan.h"
#include "Strings.h"

internal vulkan_shader LoadShader(const char *name)
{
    vulkan_shader shader = {};

    char vertPath[256];
    char fragPath[256];

    uint32 n = AppendString(vertPath, ArrayCount(vertPath), 0, "CompiledShaders/");
    n = AppendString(vertPath, ArrayCount(vertPath), n, name);
    AppendString(vertPath, ArrayCount(vertPath), n, ".vert.spv");

    n = AppendString(fragPath, ArrayCount(fragPath), 0, "CompiledShaders/");
    n = AppendString(fragPath, ArrayCount(fragPath), n, name);
    AppendString(fragPath, ArrayCount(fragPath), n, ".frag.spv");

    shader.vert = Win32ReadEntireFile(vertPath);
    shader.frag = Win32ReadEntireFile(fragPath);

    if (shader.vert.Data && shader.frag.Data)
    {
        DebugLog("Shader '%s' loaded (vert %u, frag %u bytes)\n",
               name, shader.vert.Size, shader.frag.Size);
    }
    else
    {
        DebugLog("Shader '%s' not found in CompiledShaders\n", name);
    }

    return shader;
}

internal void FreeShader(vulkan_shader *shader)
{
    Win32FreeFileMemory(shader->vert.Data);
    Win32FreeFileMemory(shader->frag.Data);
    *shader = {};
}

internal VkShaderModule CreateShaderModule(VkDevice device, void *code, uint32 codeSize)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = (const uint32_t *)code;

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        DebugLog("Fail to create shader module\n");
        return VK_NULL_HANDLE;
    }
    return module;
}

internal render_pipeline *GetPipeline(vulkan_context *context, const char *name)
{
    for (uint32 i = 0; i < context->PipelineCount; ++i)
    {
        if (StringsAreEqual(context->Pipelines[i].Name, name))
        {
            return &context->Pipelines[i];
        }
    }
    return 0;
}

internal bool32 CreateDescriptorSetLayout(vulkan_context *context, VkDescriptorSetLayoutBinding *bindings, uint32 count, VkDescriptorSetLayout *outLayout)
{
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = count;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(context->device, &layoutInfo, nullptr, outLayout) != VK_SUCCESS)
    {
        DebugLog("Fail to create descriptor set layout\n");
        return false;
    }

    DebugLog("Descriptor set layout created (%u bindings)\n", count);
    return true;
}

struct descriptor_set_bindings
{
    VkDescriptorSetLayoutBinding Bindings[MAX_PIPELINE_RESOURCES];
    uint32                       BindingCount;
};

internal bool32 CreateDescriptorPool(vulkan_context *context, render_pipeline *pipeline, descriptor_set_bindings *sets)
{
    VkDescriptorPoolSize poolSizes[MAX_PIPELINE_RESOURCES] = {};
    uint32 sizeCount = 0;
    uint32 maxSets = 0;

    for (uint32 setIndex = 0; setIndex < (uint32)Set_Count; ++setIndex)
    {
        descriptor_set_bindings *set = &sets[setIndex];
        if (!set->BindingCount)
        {
            continue;
        }

        maxSets += 1;

        for (uint32 b = 0; b < set->BindingCount; ++b)
        {
            poolSizes[sizeCount].type            = set->Bindings[b].descriptorType;
            poolSizes[sizeCount].descriptorCount = set->Bindings[b].descriptorCount;
            sizeCount++;
        }
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = sizeCount;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = maxSets;

    if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &pipeline->DescriptorPool) != VK_SUCCESS)
    {
        DebugLog("Fail to create descriptor pool\n");
        return false;
    }

    DebugLog("Descriptor pool created\n");
    return true;
}

internal bool32 CreateTextureSampler(vulkan_context *context, render_pipeline *pipeline, VkFilter filter, VkSamplerAddressMode addressMode)
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

    if (vkCreateSampler(context->device, &samplerInfo, nullptr, &pipeline->Sampler) != VK_SUCCESS)
    {
        DebugLog("Fail to create texture sampler\n");
        return false;
    }

    DebugLog("Texture sampler created\n");
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

internal void BindPipelineSet(VkCommandBuffer cmd, render_pipeline *pipeline, descriptor_set_index setIndex)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, setIndex, 1, &pipeline->Sets[setIndex], 0, nullptr);
}

internal void BindPipelineSetDynamic(VkCommandBuffer cmd, render_pipeline *pipeline, descriptor_set_index setIndex, uint32 offset)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, setIndex, 1, &pipeline->Sets[setIndex], 1, &offset);
}

internal void *FrameUniforms(render_pipeline *pipeline)
{
    return pipeline->Buffers[Set_PerFrame].Mapped;
}

internal void *BindNextObjectUniforms(VkCommandBuffer cmd, render_pipeline *pipeline, uint32 drawIndex)
{
    Assert(drawIndex < MAX_OBJECTS);
    pipeline_buffer *buffer = &pipeline->Buffers[Set_PerObject];
    uint32 offset = drawIndex * buffer->Stride;
    BindPipelineSetDynamic(cmd, pipeline, Set_PerObject, offset);
    return (uint8 *)buffer->Mapped + offset;
}

internal bool32 CreatePipelineLayout(vulkan_context *context, render_pipeline *pipeline, render_pipeline_config *config)
{

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = config->PushConstantStages;
    pushRange.offset = 0;
    pushRange.size = config->PushConstantSize;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = Set_Count;
    layoutInfo.pSetLayouts = pipeline->SetLayouts;
    if (config->PushConstantSize > 0)
    {
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
    }

    if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &pipeline->Layout) != VK_SUCCESS)
    {
        DebugLog("Fail to create pipeline layout\n");
        return false;
    }

    DebugLog("Pipeline layout created\n");
    return true;
}

internal bool32 CreateGraphicsPipeline(vulkan_context *context, render_pipeline *pipeline, render_pipeline_config *config)
{
    vulkan_shader shader = LoadShader(config->ShaderName);
    if (!shader.vert.Data || !shader.frag.Data)
    {
        DebugLog("Fail to load '%s' shader\n", config->ShaderName);
        FreeShader(&shader);
        return false;
    }

    VkShaderModule vertModule = CreateShaderModule(context->device, shader.vert.Data, shader.vert.Size);
    VkShaderModule fragModule = CreateShaderModule(context->device, shader.frag.Data, shader.frag.Size);
    if (vertModule == VK_NULL_HANDLE || fragModule == VK_NULL_HANDLE)
    {
        FreeShader(&shader);
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2] = {};
    shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertModule;
    shaderStages[0].pName  = "main";

    shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragModule;
    shaderStages[1].pName  = "main";

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = config->VertexStride;
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = config->AttributeCount;
    vertexInput.pVertexAttributeDescriptions = config->Attributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = config->Topology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = config->CullMode;
    rasterizer.frontFace = config->FrontFace;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = config->AlphaBlend ? VK_TRUE : VK_FALSE;
    if (config->AlphaBlend)
    {
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = (uint32)ArrayCount(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = config->DepthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = config->DepthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = pipeline->Layout;
    pipelineInfo.renderPass = context->renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkResult result = vkCreateGraphicsPipelines(context->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline->Handle);

    vkDestroyShaderModule(context->device, fragModule, nullptr);
    vkDestroyShaderModule(context->device, vertModule, nullptr);
    FreeShader(&shader);

    if (result != VK_SUCCESS)
    {
        DebugLog("Fail to create graphics pipeline\n");
        return false;
    }

    DebugLog("Graphics pipeline created\n");
    return true;
}

internal void DestroyRenderPipeline(vulkan_context *context, render_pipeline *pipeline);

internal render_pipeline *FailRenderPipeline(vulkan_context *context, render_pipeline *pipeline)
{
    DestroyRenderPipeline(context, pipeline);
    context->PipelineCount--;
    return 0;
}

internal bool32 WritePipelineResources(vulkan_context *context, render_pipeline *pipeline, render_pipeline_config *config)
{
    uint32 uboAlign = context->uniformBufferAlignment;

    for (uint32 i = 0; i < config->ResourceDescriptionCount; ++i)
    {
        const resource_binding_description *resourceDesc = &config->ResourcesDescription[i];

        if (resourceDesc->Type == VK_DESCRIPTOR_TYPE_SAMPLER)
        {
            WriteSamplerDescriptor(context, pipeline->Sets[resourceDesc->Set], resourceDesc->Binding, pipeline->Sampler);
            continue;
        }

        if (resourceDesc->Type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER && resourceDesc->Type != VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            continue;
        }

        pipeline_buffer *buffer = &pipeline->Buffers[resourceDesc->Set];
        if (buffer->Buffer != VK_NULL_HANDLE)
        {
            DebugLog("Pipeline '%s': set %u declares more than one buffer\n", config->ShaderName, (uint32)resourceDesc->Set);
            return false;
        }

        buffer->Stride = resourceDesc->Size;
        if (resourceDesc->Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
        {
            buffer->Stride = AlignPow2(resourceDesc->Size, uboAlign);
        }

        if (!CreateMappedBuffer(context, (VkDeviceSize)buffer->Stride * resourceDesc->Count, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &buffer->Buffer, &buffer->Memory, &buffer->Mapped))
        {
            return false;
        }

        WriteBufferDescriptor(context, pipeline->Sets[resourceDesc->Set], resourceDesc->Binding, resourceDesc->Type, buffer->Buffer, resourceDesc->Size);
    }

    return true;
}

internal render_pipeline *CreateRenderPipeline(vulkan_context *context, render_pipeline_config *config)
{
    if (config->ResourceDescriptionCount > MAX_PIPELINE_RESOURCES)
    {
        DebugLog("Pipeline '%s' has too many resources (%u, max %d)\n", config->ShaderName, config->ResourceDescriptionCount, MAX_PIPELINE_RESOURCES);
        return 0;
    }

    if (GetPipeline(context, config->ShaderName))
    {
        DebugLog("Pipeline '%s' already exists\n", config->ShaderName);
        return 0;
    }

    if (context->PipelineCount >= MAX_PIPELINES)
    {
        DebugLog("Pipeline overflow (max %d)\n", MAX_PIPELINES);
        return 0;
    }

    render_pipeline *pipeline = &context->Pipelines[context->PipelineCount++];
    *pipeline = {};
    AppendString(pipeline->Name, MAX_PIPELINE_NAME, 0, config->ShaderName);

    descriptor_set_bindings sets[Set_Count] = {};
    for (uint32 i = 0; i < config->ResourceDescriptionCount; ++i)
    {
        const resource_binding_description *resourceDesc = &config->ResourcesDescription[i];
        descriptor_set_bindings *ResourceSet = &sets[resourceDesc->Set];

        VkDescriptorSetLayoutBinding *binding = &ResourceSet->Bindings[ResourceSet->BindingCount++];
        binding->binding         = resourceDesc->Binding;
        binding->descriptorType  = resourceDesc->Type;
        binding->descriptorCount = (resourceDesc->Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER || resourceDesc->Type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC) ? 1 : resourceDesc->Count;
        binding->stageFlags      = resourceDesc->Stages;
    }

    for (uint32 setIndex = 0; setIndex < (uint32)Set_Count; ++setIndex)
    {
        if (!CreateDescriptorSetLayout(context, sets[setIndex].Bindings, sets[setIndex].BindingCount, &pipeline->SetLayouts[setIndex]))
        {
            return FailRenderPipeline(context, pipeline);
        }
    }

    if (!CreatePipelineLayout(context, pipeline, config))    return FailRenderPipeline(context, pipeline);
    if (!CreateGraphicsPipeline(context, pipeline, config))  return FailRenderPipeline(context, pipeline);
    if (!CreateDescriptorPool(context, pipeline, sets))      return FailRenderPipeline(context, pipeline);
    if (!CreateTextureSampler(context, pipeline, config->SamplerFilter, config->SamplerAddressMode)) return FailRenderPipeline(context, pipeline);

    for (uint32 setIndex = 0; setIndex < (uint32)Set_Count; ++setIndex)
    {
        if (!sets[setIndex].BindingCount)
        {
            continue;
        }

        if (!AllocateDescriptorSet(context, pipeline->DescriptorPool, pipeline->SetLayouts[setIndex], &pipeline->Sets[setIndex]))
        {
            return FailRenderPipeline(context, pipeline);
        }
    }

    if (!WritePipelineResources(context, pipeline, config)) return FailRenderPipeline(context, pipeline);

    DebugLog("Render pipeline '%s' ready (%u resources)\n", config->ShaderName, config->ResourceDescriptionCount);
    return pipeline;
}

internal void DestroyRenderPipeline(vulkan_context *context, render_pipeline *pipeline)
{
    for (uint32 setIndex = 0; setIndex < (uint32)Set_Count; ++setIndex)
    {
        pipeline_buffer *buffer = &pipeline->Buffers[setIndex];
        if (buffer->Mapped)
        {
            vkUnmapMemory(context->device, buffer->Memory);
        }
        vkDestroyBuffer(context->device, buffer->Buffer, nullptr);
        vkFreeMemory(context->device, buffer->Memory, nullptr);
        vkDestroyDescriptorSetLayout(context->device, pipeline->SetLayouts[setIndex], nullptr);
    }

    vkDestroySampler(context->device, pipeline->Sampler, nullptr);
    vkDestroyDescriptorPool(context->device, pipeline->DescriptorPool, nullptr);
    vkDestroyPipeline(context->device, pipeline->Handle, nullptr);
    vkDestroyPipelineLayout(context->device, pipeline->Layout, nullptr);
}

internal bool32 CreatePrimitivePipeline(vulkan_context *context)
{
    render_pipeline_config config = {};
    config.ShaderName = "primitive";

    config.VertexStride = KBN_VERTEX_FLOATS * (uint32)sizeof(real32);

    VkVertexInputAttributeDescription attributes[] =
    {
        { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 },
        { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * (uint32)sizeof(real32) },
        { 2, 0, VK_FORMAT_R32G32_SFLOAT,    6 * (uint32)sizeof(real32) },
    };
    config.Attributes     = attributes;
    config.AttributeCount = (uint32)ArrayCount(attributes);

    resource_binding_description resources[] =
    {
        { Set_PerFrame,     0,    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_VERTEX_BIT,   sizeof(camera_uniforms),  1 },
        { Set_PerMaterial,  0,    VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,          VK_SHADER_STAGE_FRAGMENT_BIT, 0,                        MAX_TEXTURES },
        { Set_PerMaterial,  1,    VK_DESCRIPTOR_TYPE_SAMPLER,                VK_SHADER_STAGE_FRAGMENT_BIT, 0,                        1 },
        { Set_PerObject,    0,    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(object_uniforms),  MAX_OBJECTS },
    };
    config.ResourcesDescription     = resources;
    config.ResourceDescriptionCount = (uint32)ArrayCount(resources);

    config.PushConstantSize   = (uint32)sizeof(primitive_push_constants);
    config.PushConstantStages = VK_SHADER_STAGE_VERTEX_BIT;

    config.Topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.CullMode   = VK_CULL_MODE_NONE;
    config.FrontFace  = VK_FRONT_FACE_CLOCKWISE;
    config.DepthTest  = true;
    config.DepthWrite = true;
    config.AlphaBlend = false;

    config.SamplerFilter      = VK_FILTER_LINEAR;
    config.SamplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    context->PrimitivePipeline = CreateRenderPipeline(context, &config);
    return context->PrimitivePipeline != 0;
}
