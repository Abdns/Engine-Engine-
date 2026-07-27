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
        DebugLog("Shader '%s' loaded (vert %u, frag %u bytes)\n",name, shader.vert.Size, shader.frag.Size);
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

    return true;
}

struct descriptor_set_bindings
{
    VkDescriptorSetLayoutBinding Bindings[MAX_PIPELINE_RESOURCES];
    uint32                       BindingCount;
};

// A pipeline owns exactly one set, so the pool it needs is exactly one set wide
internal bool32 CreateDescriptorPool(vulkan_context *context, render_pipeline *pipeline, descriptor_set_bindings *materialSet)
{
    if (!materialSet->BindingCount)
    {
        return true;
    }

    VkDescriptorPoolSize poolSizes[MAX_PIPELINE_RESOURCES] = {};
    for (uint32 b = 0; b < materialSet->BindingCount; ++b)
    {
        poolSizes[b].type            = materialSet->Bindings[b].descriptorType;
        poolSizes[b].descriptorCount = materialSet->Bindings[b].descriptorCount;
    }

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = materialSet->BindingCount;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &pipeline->DescriptorPool) != VK_SUCCESS)
    {
        DebugLog("Fail to create descriptor pool\n");
        return false;
    }

    return true;
}

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

// Sets 0 and 1 for the whole engine. Created once, before any pipeline, and outliving all of them
internal bool32 CreateSharedResources(vulkan_context *context)
{
    // Stage masks are the union of everything that may ever read these. The layouts are shared by
    // every pipeline, so widening one later would mean rebuilding all of them
    VkDescriptorSetLayoutBinding globalBindings[2] = {};
    globalBindings[0].binding         = 0;
    globalBindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    globalBindings[0].descriptorCount = MAX_TEXTURES;
    globalBindings[0].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    globalBindings[1].binding         = 1;
    globalBindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    globalBindings[1].descriptorCount = 1;
    globalBindings[1].stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding cameraBinding{};
    cameraBinding.binding         = 0;
    cameraBinding.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraBinding.descriptorCount = 1;
    cameraBinding.stageFlags      = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    if (!CreateDescriptorSetLayout(context, globalBindings, (uint32)ArrayCount(globalBindings), &context->GlobalSet.Layout)) return false;
    if (!CreateDescriptorSetLayout(context, &cameraBinding, 1, &context->FrameSet.Layout))                                   return false;

    VkDescriptorPoolSize poolSizes[3] = {};
    poolSizes[0].type            = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    poolSizes[0].descriptorCount = MAX_TEXTURES;
    poolSizes[1].type            = VK_DESCRIPTOR_TYPE_SAMPLER;
    poolSizes[1].descriptorCount = 1;
    poolSizes[2].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[2].descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = (uint32)ArrayCount(poolSizes);
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 2;

    if (vkCreateDescriptorPool(context->device, &poolInfo, nullptr, &context->SharedDescriptorPool) != VK_SUCCESS)
    {
        DebugLog("Fail to create shared descriptor pool\n");
        return false;
    }

    if (!AllocateDescriptorSet(context, context->SharedDescriptorPool, context->GlobalSet.Layout, &context->GlobalSet.Handle)) return false;
    if (!AllocateDescriptorSet(context, context->SharedDescriptorPool, context->FrameSet.Layout, &context->FrameSet.Handle))   return false;

    if (!CreateTextureSampler(context, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT))
    {
        return false;
    }
    WriteSamplerDescriptor(context, context->GlobalSet.Handle, 1, context->Sampler);

    // Texture views are written into binding 0 later, as assets load
    pipeline_buffer *buffer = &context->FrameBuffer;
    buffer->Stride = (uint32)sizeof(camera_uniforms);

    if (!CreateMappedBuffer(context, buffer->Stride, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, &buffer->Buffer, &buffer->Memory, &buffer->Mapped))
    {
        return false;
    }

    WriteBufferDescriptor(context, context->FrameSet.Handle, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, buffer->Buffer, buffer->Stride);

    return true;
}

internal void DestroySharedResources(vulkan_context *context)
{
    pipeline_buffer *buffer = &context->FrameBuffer;
    if (buffer->Mapped)
    {
        vkUnmapMemory(context->device, buffer->Memory);
    }
    vkDestroyBuffer(context->device, buffer->Buffer, nullptr);
    vkFreeMemory(context->device, buffer->Memory, nullptr);

    vkDestroySampler(context->device, context->Sampler, nullptr);
    vkDestroyDescriptorPool(context->device, context->SharedDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(context->device, context->FrameSet.Layout, nullptr);
    vkDestroyDescriptorSetLayout(context->device, context->GlobalSet.Layout, nullptr);

    *buffer = {};
    context->Sampler              = VK_NULL_HANDLE;
    context->SharedDescriptorPool = VK_NULL_HANDLE;
    context->FrameSet             = {};
    context->GlobalSet            = {};
}

internal void BindGlobalSet(VkCommandBuffer cmd, vulkan_context *context, render_pipeline *pipeline)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, Set_Global, 1, &context->GlobalSet.Handle, 0, nullptr);
}

internal void BindFrameSet(VkCommandBuffer cmd, vulkan_context *context, render_pipeline *pipeline)
{
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, Set_PerFrame, 1, &context->FrameSet.Handle, 0, nullptr);
}

// Does nothing while no pipeline declares per-material resources
internal void BindMaterialSet(VkCommandBuffer cmd, render_pipeline *pipeline)
{
    if (pipeline->MaterialSet.Handle == VK_NULL_HANDLE)
    {
        return;
    }

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->Layout, Set_PerMaterial, 1, &pipeline->MaterialSet.Handle, 0, nullptr);
}

internal void *FrameUniforms(vulkan_context *context)
{
    return context->FrameBuffer.Mapped;
}

internal bool32 CreatePipelineLayout(vulkan_context *context, render_pipeline *pipeline, render_pipeline_config *config)
{
    // Sets 0 and 1 are the context's own handles, so every pipeline layout comes out identical for
    // them: their bindings then survive a pipeline switch untouched
    VkDescriptorSetLayout setLayouts[Set_Count];
    setLayouts[Set_Global]      = context->GlobalSet.Layout;
    setLayouts[Set_PerFrame]    = context->FrameSet.Layout;
    setLayouts[Set_PerMaterial] = pipeline->MaterialSet.Layout;

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = config->PushConstantStages;
    pushRange.offset = 0;
    pushRange.size = config->PushConstantSize;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = Set_Count;
    layoutInfo.pSetLayouts = setLayouts;
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

    return true;
}

internal void DestroyRenderPipeline(vulkan_context *context, render_pipeline *pipeline);

internal render_pipeline *FailRenderPipeline(vulkan_context *context, render_pipeline *pipeline)
{
    DestroyRenderPipeline(context, pipeline);
    return 0;
}

internal render_pipeline *CreateRenderPipeline(vulkan_context *context, render_pipeline_config *config, render_pipeline* pipeline)
{
    if (config->ResourceDescriptionCount > MAX_PIPELINE_RESOURCES || config->AttributeCount > MAX_PIPELINE_ATTRIBUTES)
    {
        DebugLog("Pipeline '%s' has too many resources or many vertex attributes (%u, max %d)\n", config->ShaderName, config->ResourceDescriptionCount, MAX_PIPELINE_RESOURCES);
        return 0;
    }

    descriptor_set_bindings materialSet = {};
    for (uint32 i = 0; i < config->ResourceDescriptionCount; ++i)
    {
        const resource_binding_description *resourceDesc = &config->ResourcesDescription[i];

        if (resourceDesc->Set != Set_PerMaterial)
        {
            DebugLog("Pipeline '%s': set %u belongs to the context, only set %u is pipeline-owned\n", config->ShaderName, (uint32)resourceDesc->Set, (uint32)Set_PerMaterial);
            return FailRenderPipeline(context, pipeline);
        }

        VkDescriptorSetLayoutBinding *binding = &materialSet.Bindings[materialSet.BindingCount++];
        binding->binding         = resourceDesc->Binding;
        binding->descriptorType  = resourceDesc->Type;
        binding->descriptorCount = resourceDesc->Count;
        binding->stageFlags      = resourceDesc->Stages;
    }

    if (!CreateDescriptorSetLayout(context, materialSet.Bindings, materialSet.BindingCount, &pipeline->MaterialSet.Layout))
    {
        return FailRenderPipeline(context, pipeline);
    }

    if (!CreatePipelineLayout(context, pipeline, config))       return FailRenderPipeline(context, pipeline);
    if (!CreateGraphicsPipeline(context, pipeline, config))     return FailRenderPipeline(context, pipeline);
    if (!CreateDescriptorPool(context, pipeline, &materialSet)) return FailRenderPipeline(context, pipeline);

    if (materialSet.BindingCount &&
        !AllocateDescriptorSet(context, pipeline->DescriptorPool, pipeline->MaterialSet.Layout, &pipeline->MaterialSet.Handle))
    {
        return FailRenderPipeline(context, pipeline);
    }

    return pipeline;
}

internal void DestroyRenderPipeline(vulkan_context *context, render_pipeline *pipeline)
{
    // The global and per-frame layouts belong to the context and outlive every pipeline
    vkDestroyDescriptorSetLayout(context->device, pipeline->MaterialSet.Layout, nullptr);
    vkDestroyDescriptorPool(context->device, pipeline->DescriptorPool, nullptr);
    vkDestroyPipeline(context->device, pipeline->Handle, nullptr);
    vkDestroyPipelineLayout(context->device, pipeline->Layout, nullptr);

    *pipeline = {};
}

internal render_pipeline_config UnlitPipelineConfig()
{
    render_pipeline_config config = {};
    config.PipelineId = Pipeline_Unlit;
    config.ShaderName = "unlit";

    config.VertexStride = KBN_VERTEX_FLOATS * (uint32)sizeof(real32);

    config.Attributes[0] = { 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0 };
    config.Attributes[1] = { 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * (uint32)sizeof(real32) };
    config.Attributes[2] = { 2, 0, VK_FORMAT_R32G32_SFLOAT,    6 * (uint32)sizeof(real32) };
    config.AttributeCount = 3;

    // Textures and camera live in the context's sets, per-draw data rides in push constants:
    // this pipeline owns no resources of its own
    config.ResourceDescriptionCount = 0;

    config.PushConstantSize   = (uint32)sizeof(primitive_push_constants);
    config.PushConstantStages = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    config.Topology   = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    config.CullMode   = VK_CULL_MODE_NONE;
    config.FrontFace  = VK_FRONT_FACE_CLOCKWISE;
    config.DepthTest  = true;
    config.DepthWrite = true;
    config.AlphaBlend = false;

    return config;
}

internal bool32 CreatePipeline(vulkan_context* context, pipeline_type pipelineType)
{
    if ((uint32)pipelineType >= MAX_PIPELINES)
    {
        DebugLog("Invalid pipeline id %d (max %d)\n", pipelineType, MAX_PIPELINES);

        return false;
    }

    render_pipeline_config config = {};
    switch (pipelineType)
    {
        case Pipeline_Unlit: 
        {
            config = UnlitPipelineConfig(); 
        }break;

        default:
        {
            return false;
        }
    }

    render_pipeline* pipeline = &context->Pipelines[pipelineType];
    if (pipeline->Handle != VK_NULL_HANDLE)
    {
        DebugLog("Pipeline slot %d already taken by '%s'\n", pipelineType, pipeline->Name);

        return false;
    }

    *pipeline = {};
    AppendString(pipeline->Name, MAX_PIPELINE_NAME, 0, config.ShaderName);

    if (!CreateRenderPipeline(context, &config, pipeline))
    {
        return false;
    }

    context->PipelineCount++;

    return true;
}
 