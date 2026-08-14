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

    Assert(shader.vert.Data && shader.frag.Data);

    DebugLog("Shader '%s' loaded (vert %u, frag %u bytes)\n",name, shader.vert.Size, shader.frag.Size);

    return shader;
}

internal void FreeShader(vulkan_shader *shader)
{
    Win32FreeFileMemory(shader->vert.Data);
    Win32FreeFileMemory(shader->frag.Data);
    *shader = {};
}

internal VkPushConstantRange ParamsPushRange()
{
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = PIPELINE_PUSH_STAGES;
    pushRange.offset = 0;
    pushRange.size = (uint32)sizeof(push_constants);

    return pushRange;
}

internal void CreatePipelineLayout(vulkan_context *context, render_pipeline *pipeline, VkDescriptorSetLayout heapLayout)
{
    VkPushConstantRange pushRange = ParamsPushRange();

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &heapLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    VkResult result = vkCreatePipelineLayout(context->device, &layoutInfo, nullptr, &pipeline->Layout);
    Assert(result == VK_SUCCESS);
}

internal void CreateLinkedShaders(vulkan_context *context, render_pipeline *pipeline, const char *shaderName, VkDescriptorSetLayout heapLayout)
{
    vulkan_shader shader = LoadShader(shaderName);

    VkPushConstantRange pushRange = ParamsPushRange();

    VkShaderCreateInfoEXT createInfos[2] = {};

    createInfos[0].sType                  = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
    createInfos[0].flags                  = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
    createInfos[0].stage                  = VK_SHADER_STAGE_VERTEX_BIT;
    createInfos[0].nextStage              = VK_SHADER_STAGE_FRAGMENT_BIT;
    createInfos[0].codeType               = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    createInfos[0].codeSize               = shader.vert.Size;
    createInfos[0].pCode                  = shader.vert.Data;
    createInfos[0].pName                  = "main";
    createInfos[0].setLayoutCount         = 1;
    createInfos[0].pSetLayouts            = &heapLayout;
    createInfos[0].pushConstantRangeCount = 1;
    createInfos[0].pPushConstantRanges    = &pushRange;

    createInfos[1].sType                  = VK_STRUCTURE_TYPE_SHADER_CREATE_INFO_EXT;
    createInfos[1].flags                  = VK_SHADER_CREATE_LINK_STAGE_BIT_EXT;
    createInfos[1].stage                  = VK_SHADER_STAGE_FRAGMENT_BIT;
    createInfos[1].codeType               = VK_SHADER_CODE_TYPE_SPIRV_EXT;
    createInfos[1].codeSize               = shader.frag.Size;
    createInfos[1].pCode                  = shader.frag.Data;
    createInfos[1].pName                  = "main";
    createInfos[1].setLayoutCount         = 1;
    createInfos[1].pSetLayouts            = &heapLayout;
    createInfos[1].pushConstantRangeCount = 1;
    createInfos[1].pPushConstantRanges    = &pushRange;

    VkShaderEXT shaders[2] = {};

    VkResult result = context->CreateShadersEXT(context->device, 2, createInfos, nullptr, shaders);
    Assert(result == VK_SUCCESS);

    pipeline->Vert = shaders[0];
    pipeline->Frag = shaders[1];

    FreeShader(&shader);
}

internal void CreateRenderPipeline(vulkan_context *context, vulkan_resources *res, render_pipeline *pipeline, pipeline_desc *desc)
{
    pipeline->DefaultState.CullMode   = VK_CULL_MODE_NONE;
    pipeline->DefaultState.DepthTest  = desc->DepthTest;
    pipeline->DefaultState.DepthWrite = desc->DepthWrite;
    pipeline->DefaultState.AlphaBlend = desc->Blend;

    CreatePipelineLayout(context, pipeline, res->Heap.Layout);
    CreateLinkedShaders(context, pipeline, desc->ShaderName, res->Heap.Layout);
}

internal void DestroyRenderPipeline(vulkan_context *context, render_pipeline *pipeline)
{
    context->DestroyShaderEXT(context->device, pipeline->Vert, nullptr);
    context->DestroyShaderEXT(context->device, pipeline->Frag, nullptr);

    vkDestroyPipelineLayout(context->device, pipeline->Layout, nullptr);

    *pipeline = {};
}
