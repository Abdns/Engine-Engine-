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

    shader.vert = DEBUGPlatformReadEntireFile(vertPath);
    shader.frag = DEBUGPlatformReadEntireFile(fragPath);
    shader.valid = (shader.vert.Contents != 0) && (shader.frag.Contents != 0);

    if (shader.valid)
    {
        DebugLog("Shader '%s' loaded (vert %u, frag %u bytes)\n",
               name, shader.vert.ContentsSize, shader.frag.ContentsSize);
    }
    else
    {
        DebugLog("Shader '%s' not found in CompiledShaders\n", name);
    }

    return shader;
}

internal void FreeShader(vulkan_shader *shader)
{
    DEBUGPlatformFreeFileMemory(shader->vert.Contents);
    DEBUGPlatformFreeFileMemory(shader->frag.Contents);
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

internal bool32 AppendDescriptorSlot(render_pipeline_config *config, descriptor_frequency freq, uint32 binding, VkDescriptorType type, VkShaderStageFlags stages)
{
    descriptor_set_slots *slots = &config->Sets[freq];
    if (slots->BindingCount >= MAX_DESCRIPTOR_BINDINGS)
    {
        DebugLog("Descriptor slot overflow (max %d)\n", MAX_DESCRIPTOR_BINDINGS);
        return false;
    }

    VkDescriptorSetLayoutBinding *slot = &slots->Bindings[slots->BindingCount];
    slot->binding = binding;
    slot->descriptorType = type;
    slot->descriptorCount = 1;
    slot->stageFlags = stages;
    slot->pImmutableSamplers = nullptr;

    slots->BindingCount++;
    return true;
}

internal bool32 AddTextureSlot(render_pipeline_config *config, descriptor_frequency freq, uint32 binding, VkShaderStageFlags stages)
{
    return AppendDescriptorSlot(config, freq, binding, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, stages);
}

internal bool32 AddUniformSlot(render_pipeline_config *config, descriptor_frequency freq, uint32 binding, VkShaderStageFlags stages)
{
    return AppendDescriptorSlot(config, freq, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, stages);
}

internal bool32 AddDynamicUniformSlot(render_pipeline_config *config, descriptor_frequency freq, uint32 binding, VkShaderStageFlags stages)
{
    return AppendDescriptorSlot(config, freq, binding, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, stages);
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

internal bool32 CreateSetLayouts(vulkan_context *context, render_pipeline *pipeline, render_pipeline_config *config)
{
    for (uint32 freq = 0; freq < (uint32)Frequency_Count; ++freq)
    {
        descriptor_set_slots *slots = &config->Sets[freq];
        if (!CreateDescriptorSetLayout(context, slots->Bindings, slots->BindingCount, &pipeline->SetLayouts[freq]))
        {
            return false;
        }
    }
    return true;
}

internal uint32 FrequencySetCount(descriptor_frequency freq)
{
    if (freq == Frequency_PerMaterial)
    {
        return MAX_TEXTURES;
    }
    return 1;
}

internal bool32 CreateDescriptorPool(vulkan_context *context, render_pipeline *pipeline, render_pipeline_config *config)
{
    VkDescriptorPoolSize poolSizes[Frequency_Count * MAX_DESCRIPTOR_BINDINGS] = {};
    uint32 sizeCount = 0;
    uint32 maxSets = 0;

    for (uint32 freq = 0; freq < (uint32)Frequency_Count; ++freq)
    {
        descriptor_set_slots *slots = &config->Sets[freq];
        if (slots->BindingCount == 0)
        {
            continue;
        }

        uint32 setsForFreq = FrequencySetCount((descriptor_frequency)freq);
        maxSets += setsForFreq;

        for (uint32 b = 0; b < slots->BindingCount; ++b)
        {
            poolSizes[sizeCount].type = slots->Bindings[b].descriptorType;
            poolSizes[sizeCount].descriptorCount = slots->Bindings[b].descriptorCount * setsForFreq;
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

internal void WriteImageSlot(vulkan_context *context, VkDescriptorSet set, uint32 binding, VkImageView view, VkSampler sampler)
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = view;
    imageInfo.sampler = sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = binding;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(context->device, 1, &write, 0, nullptr);
}

internal void WriteBufferSlot(vulkan_context *context, VkDescriptorSet set, uint32 binding, VkBuffer buffer, VkDeviceSize range)
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
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;

    vkUpdateDescriptorSets(context->device, 1, &write, 0, nullptr);
}

internal void WriteDynamicBufferSlot(vulkan_context *context, VkDescriptorSet set, uint32 binding, VkBuffer buffer, VkDeviceSize range)
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
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
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

internal void WriteTextureDescriptor(vulkan_context *context, render_pipeline *pipeline, gpu_texture *texture)
{
    WriteImageSlot(context, texture->DescriptorSet, 0, texture->View, pipeline->Sampler);
}

internal bool32 CreateTextureDescriptorSet(vulkan_context *context, render_pipeline *pipeline, gpu_texture *texture)
{
    if (!AllocateDescriptorSet(context, pipeline->DescriptorPool, pipeline->SetLayouts[Frequency_PerMaterial], &texture->DescriptorSet))
    {
        return false;
    }

    WriteTextureDescriptor(context, pipeline, texture);
    return true;
}

internal bool32 SetPipelineSampler(vulkan_context *context, render_pipeline *pipeline, VkFilter filter, VkSamplerAddressMode addressMode)
{
    vkDeviceWaitIdle(context->device);

    VkSampler oldSampler = pipeline->Sampler;
    if (!CreateTextureSampler(context, pipeline, filter, addressMode))
    {
        pipeline->Sampler = oldSampler;
        return false;
    }

    for (uint32 i = 0; i < context->TextureCount; ++i)
    {
        WriteTextureDescriptor(context, pipeline, &context->Textures[i]);
    }

    vkDestroySampler(context->device, oldSampler, nullptr);

    DebugLog("Pipeline sampler replaced\n");
    return true;
}

internal void BindDescriptorSet(VkCommandBuffer cmd, render_pipeline *pipeline, descriptor_frequency freq, VkDescriptorSet set)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, freq, 1, &set, 0, nullptr);
}

internal void BindDynamicDescriptorSet(VkCommandBuffer cmd, render_pipeline *pipeline, descriptor_frequency freq, VkDescriptorSet set, uint32 offset)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, freq, 1, &set, 1, &offset);
}

internal bool32 CreatePipelineLayout(vulkan_context *context, render_pipeline *pipeline, render_pipeline_config *config)
{

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = config->PushConstantStages;
    pushRange.offset = 0;
    pushRange.size = config->PushConstantSize;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = Frequency_Count;
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
    if (!shader.valid)
    {
        DebugLog("Fail to load '%s' shader\n", config->ShaderName);
        return false;
    }

    VkShaderModule vertModule = CreateShaderModule(context->device, shader.vert.Contents, shader.vert.ContentsSize);
    VkShaderModule fragModule = CreateShaderModule(context->device, shader.frag.Contents, shader.frag.ContentsSize);
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

internal bool32 CreateRenderPipeline(vulkan_context *context, render_pipeline *pipeline, render_pipeline_config *config)
{
    if (!CreateSetLayouts(context, pipeline, config)) return false;
    if (!CreatePipelineLayout(context, pipeline, config)) return false;
    if (!CreateGraphicsPipeline(context, pipeline, config)) return false;
    if (!CreateDescriptorPool(context, pipeline, config)) return false;
    if (!CreateTextureSampler(context, pipeline, config->SamplerFilter, config->SamplerAddressMode)) return false;

    DebugLog("Render pipeline '%s' ready\n", config->ShaderName);
    return true;
}

internal void DestroyRenderPipeline(vulkan_context *context, render_pipeline *pipeline)
{
    vkUnmapMemory(context->device, pipeline->CameraBufferMemory);
    vkDestroyBuffer(context->device, pipeline->CameraBuffer, nullptr);
    vkFreeMemory(context->device, pipeline->CameraBufferMemory, nullptr);
    vkUnmapMemory(context->device, pipeline->ObjectBufferMemory);
    vkDestroyBuffer(context->device, pipeline->ObjectBuffer, nullptr);
    vkFreeMemory(context->device, pipeline->ObjectBufferMemory, nullptr);

    vkDestroySampler(context->device, pipeline->Sampler, nullptr);
    vkDestroyDescriptorPool(context->device, pipeline->DescriptorPool, nullptr);
    for (uint32 freq = 0; freq < (uint32)Frequency_Count; ++freq)
    {
        vkDestroyDescriptorSetLayout(context->device, pipeline->SetLayouts[freq], nullptr);
    }
    vkDestroyPipeline(context->device, pipeline->Handle, nullptr);
    vkDestroyPipelineLayout(context->device, pipeline->Layout, nullptr);
}

internal bool32 CreateCubePipeline(vulkan_context *context)
{
    render_pipeline_config config = {};
    config.ShaderName = "cube";

    config.VertexStride   = 8 * (uint32)sizeof(real32);
    config.AttributeCount = 3;
    config.Attributes[0].location = 0;
    config.Attributes[0].binding  = 0;
    config.Attributes[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    config.Attributes[0].offset   = 0;
    config.Attributes[1].location = 1;
    config.Attributes[1].binding  = 0;
    config.Attributes[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    config.Attributes[1].offset   = 3 * (uint32)sizeof(real32);
    config.Attributes[2].location = 2;
    config.Attributes[2].binding  = 0;
    config.Attributes[2].format   = VK_FORMAT_R32G32_SFLOAT;
    config.Attributes[2].offset   = 6 * (uint32)sizeof(real32);

    AddUniformSlot(&config,        Frequency_PerFrame,    0, VK_SHADER_STAGE_VERTEX_BIT);
    AddTextureSlot(&config,        Frequency_PerMaterial, 0, VK_SHADER_STAGE_FRAGMENT_BIT);
    AddDynamicUniformSlot(&config, Frequency_PerObject,   0, VK_SHADER_STAGE_FRAGMENT_BIT);

    config.PushConstantSize   = (uint32)sizeof(cube_push_constants);
    config.PushConstantStages = VK_SHADER_STAGE_VERTEX_BIT;

    config.Topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.CullMode   = VK_CULL_MODE_NONE;
    config.FrontFace  = VK_FRONT_FACE_CLOCKWISE;
    config.DepthTest  = true;
    config.DepthWrite = true;
    config.AlphaBlend = false;

    config.SamplerFilter      = VK_FILTER_LINEAR;
    config.SamplerAddressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    render_pipeline *pipeline = &context->Pipeline;
    if (!CreateRenderPipeline(context, pipeline, &config)) return false;

    if (!CreateMappedBuffer(context, sizeof(camera_uniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &pipeline->CameraBuffer, &pipeline->CameraBufferMemory, &pipeline->CameraMapped)) return false;
    if (!CreateMappedBuffer(context, OBJECT_STRIDE * MAX_OBJECTS, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &pipeline->ObjectBuffer, &pipeline->ObjectBufferMemory, &pipeline->ObjectMapped)) return false;

    if (!AllocateDescriptorSet(context, pipeline->DescriptorPool, pipeline->SetLayouts[Frequency_PerFrame], &pipeline->CameraSet)) return false;
    if (!AllocateDescriptorSet(context, pipeline->DescriptorPool, pipeline->SetLayouts[Frequency_PerObject], &pipeline->ObjectSet)) return false;

    WriteBufferSlot(context, pipeline->CameraSet, 0, pipeline->CameraBuffer, sizeof(camera_uniforms));
    WriteDynamicBufferSlot(context, pipeline->ObjectSet, 0, pipeline->ObjectBuffer, sizeof(object_uniforms));

    return true;
}
