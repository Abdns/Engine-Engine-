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

internal bool32 CreatePipelineLayout(vulkan_context *context, render_pipeline *pipeline, VkDescriptorSetLayout setLayout)
{
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = PIPELINE_PUSH_STAGES;
    pushRange.offset = 0;
    pushRange.size = (uint32)sizeof(draw_push_constants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &setLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &pipeline->Layout) != VK_SUCCESS)
    {
        DebugLog("Fail to create pipeline layout\n");
        return false;
    }

    return true;
}

internal bool32 CreateGraphicsPipeline(vulkan_context *context, render_pipeline *pipeline, VkRenderPass renderPass)
{
    vulkan_shader shader = LoadShader(pipeline->Desc.ShaderName);
    if (!shader.vert.Data || !shader.frag.Data)
    {
        DebugLog("Fail to load '%s' shader\n", pipeline->Desc.ShaderName);
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

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = PIPELINE_TOPOLOGY;
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
    rasterizer.cullMode = pipeline->DefaultState.CullMode;
    rasterizer.frontFace = PIPELINE_FRONT_FACE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = pipeline->DefaultState.AlphaBlend;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkDynamicState dynamicStates[16];
    uint32         dynamicStateCount = 0;

    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_VIEWPORT;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_SCISSOR;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_CULL_MODE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_FRONT_FACE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE;
    dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_DEPTH_COMPARE_OP;

    if (context->DynamicBlend)
    {
        dynamicStates[dynamicStateCount++] = VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT;
    }

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = dynamicStateCount;
    dynamicState.pDynamicStates = dynamicStates;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = pipeline->DefaultState.DepthTest;
    depthStencil.depthWriteEnable = pipeline->DefaultState.DepthWrite;
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
    pipelineInfo.renderPass = renderPass;
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

internal void DestroyRenderPipeline(vulkan_context *context, render_pipeline *pipeline)
{
    vkDestroyPipeline(context->device, pipeline->Handle, nullptr);
    vkDestroyPipelineLayout(context->device, pipeline->Layout, nullptr);

    *pipeline = {};
}

internal bool32 BuildPipeline(vulkan_context *context, render_pipeline *pipeline, pipeline_desc *desc, VkRenderPass renderPass, VkDescriptorSetLayout setLayout)
{
    pipeline->Desc = *desc;

    pipeline->DefaultState.CullMode   = VK_CULL_MODE_NONE;
    pipeline->DefaultState.DepthTest  = desc->DepthTest;
    pipeline->DefaultState.DepthWrite = desc->DepthWrite;
    pipeline->DefaultState.AlphaBlend = VK_FALSE;

    if (!CreatePipelineLayout(context, pipeline, setLayout) || !CreateGraphicsPipeline(context, pipeline, renderPass))
    {
        DestroyRenderPipeline(context, pipeline);
        return false;
    }

    return true;
}
 