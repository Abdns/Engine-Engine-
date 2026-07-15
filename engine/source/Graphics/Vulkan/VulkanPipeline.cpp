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

internal VkDescriptorType DescriptorTypeFor(resource_kind kind)
{
    switch (kind)
    {
        case Resource_Uniform:        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case Resource_UniformDynamic: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case Resource_Texture:        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
}

internal uint32 FrequencySetCount(descriptor_frequency freq)
{
    if (freq == Frequency_PerMaterial)
    {
        return MAX_TEXTURES;
    }
    return 1;
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

internal bool32 CreateSetLayouts(vulkan_context *context, render_pipeline *pipeline)
{
    for (uint32 freq = 0; freq < (uint32)Frequency_Count; ++freq)
    {
        VkDescriptorSetLayoutBinding bindings[MAX_PIPELINE_RESOURCES] = {};
        uint32 count = 0;

        for (uint32 i = 0; i < pipeline->ResourceCount; ++i)
        {
            resource_slot *slot = &pipeline->Resources[i].Slot;
            if ((uint32)slot->Frequency != freq)
            {
                continue;
            }

            bindings[count].binding         = slot->Binding;
            bindings[count].descriptorType  = DescriptorTypeFor(slot->Kind);
            bindings[count].descriptorCount = 1;
            bindings[count].stageFlags      = slot->Stages;
            count++;
        }

        if (!CreateDescriptorSetLayout(context, bindings, count, &pipeline->SetLayouts[freq]))
        {
            return false;
        }
    }
    return true;
}

internal bool32 CreateDescriptorPool(vulkan_context *context, render_pipeline *pipeline)
{
    VkDescriptorPoolSize poolSizes[MAX_PIPELINE_RESOURCES] = {};
    uint32 sizeCount = 0;
    uint32 maxSets = 0;

    for (uint32 freq = 0; freq < (uint32)Frequency_Count; ++freq)
    {
        uint32 setsForFreq = FrequencySetCount((descriptor_frequency)freq);
        bool32 hasSlots = false;

        for (uint32 i = 0; i < pipeline->ResourceCount; ++i)
        {
            resource_slot *slot = &pipeline->Resources[i].Slot;
            if ((uint32)slot->Frequency != freq)
            {
                continue;
            }

            hasSlots = true;
            poolSizes[sizeCount].type = DescriptorTypeFor(slot->Kind);
            poolSizes[sizeCount].descriptorCount = setsForFreq;
            sizeCount++;
        }

        if (hasSlots)
        {
            maxSets += setsForFreq;
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

internal void WriteBufferSlot(vulkan_context *context, VkDescriptorSet set, uint32 binding, VkDescriptorType type, VkBuffer buffer, VkDeviceSize range)
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

internal bool32 CreatePipelineResources(vulkan_context *context, render_pipeline *pipeline)
{
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(context->physicalDevice, &props);
    uint32 uboAlign = (uint32)props.limits.minUniformBufferOffsetAlignment;

    for (uint32 freq = 0; freq < (uint32)Frequency_Count; ++freq)
    {
        bool32 needsSet = false;
        for (uint32 i = 0; i < pipeline->ResourceCount; ++i)
        {
            resource_slot *slot = &pipeline->Resources[i].Slot;
            if ((uint32)slot->Frequency == freq && slot->Kind != Resource_Texture)
            {
                needsSet = true;
            }
        }

        if (!needsSet)
        {
            continue;
        }

        if (!AllocateDescriptorSet(context, pipeline->DescriptorPool, pipeline->SetLayouts[freq], &pipeline->Sets[freq]))
        {
            return false;
        }
    }

    for (uint32 i = 0; i < pipeline->ResourceCount; ++i)
    {
        pipeline_resource *res = &pipeline->Resources[i];
        resource_slot *slot = &res->Slot;

        if (slot->Kind == Resource_Texture)
        {
            continue;
        }

        res->Stride = slot->Size;
        if (slot->Kind == Resource_UniformDynamic)
        {
            res->Stride = (slot->Size + uboAlign - 1) & ~(uboAlign - 1);
        }

        if (!CreateMappedBuffer(context, (VkDeviceSize)res->Stride * slot->Count, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &res->Buffer, &res->Memory, &res->Mapped))
        {
            return false;
        }

        WriteBufferSlot(context, pipeline->Sets[slot->Frequency], slot->Binding, DescriptorTypeFor(slot->Kind), res->Buffer, slot->Size);
    }

    return true;
}

internal pipeline_resource *FindPipelineResource(render_pipeline *pipeline, descriptor_frequency freq, uint32 binding)
{
    for (uint32 i = 0; i < pipeline->ResourceCount; ++i)
    {
        pipeline_resource *res = &pipeline->Resources[i];
        if (res->Slot.Frequency == freq && res->Slot.Binding == binding)
        {
            return res;
        }
    }

    Assert(!"Pipeline resource not found");
    return 0;
}

internal void *PipelineResourceData(render_pipeline *pipeline, descriptor_frequency freq, uint32 binding, uint32 element)
{
    pipeline_resource *res = FindPipelineResource(pipeline, freq, binding);
    return (uint8 *)res->Mapped + element * res->Stride;
}

internal uint32 PipelineDynamicOffset(render_pipeline *pipeline, descriptor_frequency freq, uint32 binding, uint32 element)
{
    return element * FindPipelineResource(pipeline, freq, binding)->Stride;
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

internal void BindDescriptorSet(VkCommandBuffer cmd, render_pipeline *pipeline, descriptor_frequency freq, VkDescriptorSet set)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, freq, 1, &set, 0, nullptr);
}

internal void BindPipelineSet(VkCommandBuffer cmd, render_pipeline *pipeline, descriptor_frequency freq)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, freq, 1, &pipeline->Sets[freq], 0, nullptr);
}

internal void BindPipelineSetDynamic(VkCommandBuffer cmd, render_pipeline *pipeline, descriptor_frequency freq, uint32 offset)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, freq, 1, &pipeline->Sets[freq], 1, &offset);
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

internal render_pipeline *CreateRenderPipeline(vulkan_context *context, render_pipeline_config *config)
{
    Assert(config->ResourceCount <= MAX_PIPELINE_RESOURCES);

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

    pipeline->ResourceCount = config->ResourceCount;
    for (uint32 i = 0; i < config->ResourceCount; ++i)
    {
        pipeline->Resources[i].Slot = config->Resources[i];
    }

    if (!CreateSetLayouts(context, pipeline))                return 0;
    if (!CreatePipelineLayout(context, pipeline, config))    return 0;
    if (!CreateGraphicsPipeline(context, pipeline, config))  return 0;
    if (!CreateDescriptorPool(context, pipeline))            return 0;
    if (!CreateTextureSampler(context, pipeline, config->SamplerFilter, config->SamplerAddressMode)) return 0;
    if (!CreatePipelineResources(context, pipeline))         return 0;

    DebugLog("Render pipeline '%s' ready (%u resources)\n", config->ShaderName, config->ResourceCount);
    return pipeline;
}

internal void DestroyRenderPipeline(vulkan_context *context, render_pipeline *pipeline)
{
    for (uint32 i = 0; i < pipeline->ResourceCount; ++i)
    {
        pipeline_resource *res = &pipeline->Resources[i];
        if (res->Buffer == VK_NULL_HANDLE)
        {
            continue;
        }
        vkUnmapMemory(context->device, res->Memory);
        vkDestroyBuffer(context->device, res->Buffer, nullptr);
        vkFreeMemory(context->device, res->Memory, nullptr);
    }

    vkDestroySampler(context->device, pipeline->Sampler, nullptr);
    vkDestroyDescriptorPool(context->device, pipeline->DescriptorPool, nullptr);
    for (uint32 freq = 0; freq < (uint32)Frequency_Count; ++freq)
    {
        vkDestroyDescriptorSetLayout(context->device, pipeline->SetLayouts[freq], nullptr);
    }
    vkDestroyPipeline(context->device, pipeline->Handle, nullptr);
    vkDestroyPipelineLayout(context->device, pipeline->Layout, nullptr);
}

internal bool32 CreatePrimitivePipeline(vulkan_context *context)
{
    render_pipeline_config config = {};
    config.ShaderName = "primitive";

    config.VertexStride   = KBN_VERTEX_FLOATS * (uint32)sizeof(real32);
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

    resource_slot resources[] =
    {
        { Frequency_PerFrame,     0,    Resource_Uniform,        VK_SHADER_STAGE_VERTEX_BIT,   sizeof(camera_uniforms),  1 },
        { Frequency_PerMaterial,  0,    Resource_Texture,        VK_SHADER_STAGE_FRAGMENT_BIT, 0,                        1 },
        { Frequency_PerObject,    0,    Resource_UniformDynamic, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(object_uniforms),  MAX_OBJECTS },
    };
    config.Resources     = resources;
    config.ResourceCount = (uint32)ArrayCount(resources);

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

    return CreateRenderPipeline(context, &config) != 0;
}
